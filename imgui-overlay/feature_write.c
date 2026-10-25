/*
 * feature_write.c - 复选框功能本体（C 语言）
 *
 * 与仓库里的终端版 modifier_main.c 同一套逻辑和参数：
 *   包名 com.rekoo.pubgm，模块 libil2cpp.so，
 *   指针链 0x56CFF0 -> 0x30 -> 0x4B8 -> 0x360 -> 0x170 -> 0x39C，写 110。
 * 写法同样按修正版：指针按 64 位读、首偏移加模块基址、包名精确匹配。
 * gamemodifier.c 未改动。
 */

#include "feature_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#define TARGET_PACKAGE "com.rekoo.pubgm"
#define TARGET_MODULE  "libil2cpp.so"
#define WRITE_VALUE    110

static const uintptr_t OFFSETS[] = {
    0x56CFF0, 0x30, 0x4B8, 0x360, 0x170, 0x39C
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

static uintptr_t FindModuleBase(pid_t pid, const char *moduleName)
{
    char mapsPath[256];
    snprintf(mapsPath, sizeof(mapsPath), "/proc/%d/maps", (int)pid);

    FILE *fp = fopen(mapsPath, "r");
    if (!fp) return 0;

    char line[1024];
    uintptr_t base = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, moduleName) == NULL) continue;
        base = (uintptr_t)strtoull(line, NULL, 16);
        break;
    }
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

/* 成功返回最终地址，失败返回 0 并通过 failStep/failErrno 说明原因 */
static uintptr_t ResolvePointerChain(pid_t pid, uintptr_t moduleBase,
                                     int *failStep, int *failErrno, int *failNull)
{
    uintptr_t current = moduleBase + OFFSETS[0];
    *failStep = 0;
    *failErrno = 0;
    *failNull = 0;

    for (size_t i = 1; i < OFFSET_COUNT; i++) {
        uint64_t ptr = 0;
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

int Feature_ApplyWrite(char *message, size_t messageSize)
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

    uintptr_t moduleBase = FindModuleBase(pid, TARGET_MODULE);
    if (moduleBase == 0) {
        snprintf(message, messageSize, "失败：未找到 %s 基址", TARGET_MODULE);
        return 1;
    }

    int failStep = 0, failErrno = 0, failNull = 0;
    uintptr_t finalAddr = ResolvePointerChain(pid, moduleBase, &failStep, &failErrno, &failNull);
    if (finalAddr == 0) {
        if (failNull) {
            snprintf(message, messageSize, "失败：指针链第 %d 步为空，偏移可能已失效", failStep);
        } else {
            snprintf(message, messageSize, "失败：指针链第 %d 步读取失败（%s）",
                     failStep, strerror(failErrno));
        }
        return 1;
    }

    uint32_t oldValue = 0;
    if (ReadProcessMemory(pid, finalAddr, &oldValue, sizeof(oldValue)) != 0) {
        snprintf(message, messageSize, "失败：读取当前值失败（%s）", strerror(errno));
        return 1;
    }

    uint32_t newValue = WRITE_VALUE;
    if (WriteProcessMemory(pid, finalAddr, &newValue, sizeof(newValue)) != 0) {
        snprintf(message, messageSize, "失败：写入失败（%s）", strerror(errno));
        return 1;
    }

    uint32_t checkValue = 0;
    if (ReadProcessMemory(pid, finalAddr, &checkValue, sizeof(checkValue)) == 0
            && checkValue != (uint32_t)WRITE_VALUE) {
        snprintf(message, messageSize, "失败：写入后读回 %u，与 110 不一致",
                 (unsigned int)checkValue);
        return 1;
    }

    snprintf(message, messageSize, "成功：已写入 110（原值 %u）", (unsigned int)oldValue);
    return 0;
}
