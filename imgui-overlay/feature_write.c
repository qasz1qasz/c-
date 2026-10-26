/*
 * feature_write.c - 动态修改功能本体（C 语言）
 *
 * 用户提供的 GameGuardian Lua 脚本（S_Pointer + gg.addListItems）
 * 逐行翻成的 C，开关开启后由主程序每秒调一次，相当于 Lua 里的 freeze=true；
 * 写入值由界面输入框传入，默认 120（脚本里的 value）。
 *
 * Lua -> C 对照：
 *   gg.getRangesList + state "Cb" + 名字 "libUE4.so:bss"
 *     -> 在 /proc/<pid>/maps 里找 libUE4.so 文件映射之后紧跟的第一个
 *        可写匿名映射，那就是它的 .bss 段，用它的起始地址当锚点
 *        （注意：不是 libUE4.so 的装载基址，Lua 锚的是 bss）
 *   指针链 {0xB3D7F0, 0x0, 0x3C8, 0x2F4}，_bit=true
 *     -> 首地址 = bss + 0xB3D7F0，之后每级按 64 位读指针再加下一偏移，
 *        共读 3 次，最终地址 = 最后指针 + 0x2F4
 *   gg.addListItems({flags = 16, value = 120, freeze = true})
 *     -> flags 16 是 GG 的 TYPE_FLOAT：把数值按 float 写入（不是整数），
 *        freeze 对应这里的每秒重复写入
 *   Lua 里那句 print 是作者水印（"缤:" + 一个常数），无功能，未翻译。
 *
 * 目标包名 com.tencent.tmgp.pubgmhd（和平精英）由用户指定
 * （Lua 在 GG 里是手动选进程的，没有写包名）。
 * gamemodifier.c 未改动。
 */

#define _POSIX_C_SOURCE 200809L /* pread/pwrite 的声明 */

#include "feature_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#define TARGET_PACKAGE "com.tencent.tmgp.pubgmhd"
#define TARGET_MODULE  "libUE4.so"

static const uintptr_t OFFSETS[] = {
    0xB3D7F0, 0x0, 0x3C8, 0x2F4
};
#define OFFSET_COUNT (sizeof(OFFSETS) / sizeof(OFFSETS[0]))

static pid_t FindProcessByPackage(const char *packageName)
{
    DIR *dir = opendir("/proc");
    if (!dir) return -1;

    pid_t result = -1;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (name[0] < '0' || name[0] > '9') continue;

        char path[512];
        snprintf(path, sizeof(path), "/proc/%s/cmdline", name);

        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;

        char cmdline[512];
        ssize_t len = read(fd, cmdline, sizeof(cmdline) - 1);
        close(fd);

        if (len <= 0) continue;
        cmdline[len] = '\0';

        if (strcmp(cmdline, packageName) == 0) {
            result = (pid_t)atoi(name);
            break;
        }
    }
    closedir(dir);
    return result;
}

/*
 * 在 maps 里找 moduleName 的 .bss 段起始地址：
 * 先出现该模块的文件映射，紧接着的第一个「匿名 + 可写」映射就是 bss。
 * GG 里这个范围的名字是 "libUE4.so:bss"、state 是 "Cb"（C BSS），
 * 且它只取可写范围（type 第二个字符是 'w'），与这里 perms[1] == 'w' 对应。
 * 找不到返回 0。单独拆出来是为了能在电脑上拿样本 maps 单测。
 */
static uintptr_t FindModuleBssBaseFromMaps(FILE *fp, const char *moduleName)
{
    char line[1024];
    int seenModule = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        uintptr_t start = (uintptr_t)strtoull(line, NULL, 16);

        char perms[8] = {0};
        char path[512] = {0};
        int n = sscanf(line, "%*s %7s %*s %*s %*s %511[^\n]", perms, path);
        bool hasPath = (n == 2 && path[0] != '\0');

        if (hasPath) {
            /* 路径尾巴可能带 " (deleted)"，先去掉 */
            char *deleted = strstr(path, " (deleted)");
            if (deleted) *deleted = '\0';
        }

        /* 是不是目标模块的文件映射：比对路径最后一段文件名 */
        bool isModuleFile = false;
        if (hasPath) {
            const char *slash = strrchr(path, '/');
            const char *base = slash ? slash + 1 : path;
            isModuleFile = (strcmp(base, moduleName) == 0);
        }

        /* 匿名映射：没有路径，或被标成 [anon:.bss] 之类 */
        bool isAnon = !hasPath || strstr(path, ".bss") != NULL;

        if (isModuleFile) {
            seenModule = 1;
            continue;
        }
        if (seenModule && isAnon) {
            if (perms[1] == 'w') return start;   /* 找到了：bss 起始 */
            continue;                            /* 不可写的匿名段先跳过 */
        }
        if (hasPath) seenModule = 0;             /* 换成别的文件了，重置 */
    }
    return 0;
}

static uintptr_t FindModuleBssBase(pid_t pid, const char *moduleName)
{
    char mapsPath[256];
    snprintf(mapsPath, sizeof(mapsPath), "/proc/%d/maps", (int)pid);

    FILE *fp = fopen(mapsPath, "r");
    if (!fp) return 0;

    uintptr_t base = FindModuleBssBaseFromMaps(fp, moduleName);
    fclose(fp);
    return base;
}

static int ReadProcessMemory(pid_t pid, uintptr_t address, void *buffer, size_t size)
{
    char memPath[256];
    snprintf(memPath, sizeof(memPath), "/proc/%d/mem", (int)pid);

    int fd = open(memPath, O_RDONLY);
    if (fd < 0) return -1;

    ssize_t ret = pread(fd, buffer, size, (off_t)address);
    int savedErrno = errno;
    close(fd);
    errno = savedErrno;
    return (ret == (ssize_t)size) ? 0 : -1;
}

static int WriteProcessMemory(pid_t pid, uintptr_t address, const void *buffer, size_t size)
{
    char memPath[256];
    snprintf(memPath, sizeof(memPath), "/proc/%d/mem", (int)pid);

    int fd = open(memPath, O_RDWR);
    if (fd < 0) return -1;

    ssize_t ret = pwrite(fd, buffer, size, (off_t)address);
    int savedErrno = errno;
    close(fd);
    errno = savedErrno;
    return (ret == (ssize_t)size) ? 0 : -1;
}

/* 成功返回最终地址，失败返回 0 并通过 failStep/failErrno/failNull 说明原因 */
static uintptr_t ResolvePointerChain(pid_t pid, uintptr_t bssBase,
                                     int *failStep, int *failErrno, int *failNull)
{
    uintptr_t current = bssBase + OFFSETS[0];
    *failStep = 0;
    *failErrno = 0;
    *failNull = 0;

    for (size_t i = 1; i < OFFSET_COUNT; i++) {
        uint64_t ptr = 0;   /* Lua _bit=true：指针按 64 位（GG 的 QWORD）读 */
        if (ReadProcessMemory(pid, current, &ptr, sizeof(ptr)) != 0) {
            *failStep = (int)i;
            *failErrno = errno;
            return 0;
        }
        if (ptr == 0) {
            *failStep = (int)i;
            *failNull = 1;
            return 0;
        }
        if (i == OFFSET_COUNT - 1) {
            return (uintptr_t)ptr + OFFSETS[i];
        }
        current = (uintptr_t)ptr + OFFSETS[i];
    }
    return 0;
}

int Feature_ApplyWrite(float value, char *message, size_t messageSize)
{
    if (geteuid() != 0) {
        snprintf(message, messageSize, "失败：不是 root 运行，读不了游戏内存，请用 su 后再启动");
        return 1;
    }

    pid_t pid = FindProcessByPackage(TARGET_PACKAGE);
    if (pid <= 0) {
        snprintf(message, messageSize, "失败：未找到游戏进程，请先打开游戏");
        return 1;
    }

    uintptr_t bssBase = FindModuleBssBase(pid, TARGET_MODULE);
    if (bssBase == 0) {
        snprintf(message, messageSize, "失败：未找到 %s 的 bss 段", TARGET_MODULE);
        return 1;
    }

    int failStep = 0, failErrno = 0, failNull = 0;
    uintptr_t finalAddr = ResolvePointerChain(pid, bssBase, &failStep, &failErrno, &failNull);
    if (finalAddr == 0) {
        if (failNull) {
            snprintf(message, messageSize, "失败：指针链第 %d 步为空，偏移可能已失效", failStep);
        } else {
            snprintf(message, messageSize, "失败：指针链第 %d 步读取失败（%s）",
                     failStep, strerror(failErrno));
        }
        return 1;
    }

    float oldValue = 0.0f;
    if (ReadProcessMemory(pid, finalAddr, &oldValue, sizeof(oldValue)) != 0) {
        snprintf(message, messageSize, "失败：读取当前值失败（%s）", strerror(errno));
        return 1;
    }

    float newValue = value;
    if (WriteProcessMemory(pid, finalAddr, &newValue, sizeof(newValue)) != 0) {
        snprintf(message, messageSize, "失败：写入失败（%s）", strerror(errno));
        return 1;
    }

    float checkValue = 0.0f;
    if (ReadProcessMemory(pid, finalAddr, &checkValue, sizeof(checkValue)) == 0
            && checkValue != value) {
        snprintf(message, messageSize, "失败：写入后读回 %g，与 %g 不一致",
                 (double)checkValue, (double)value);
        return 1;
    }

    snprintf(message, messageSize, "成功：已写入 %g（float，原值 %g）",
             (double)value, (double)oldValue);
    return 0;
}
