/*
 * GameModifier - 终端可执行版（独立新文件，不依赖 gamemodifier.c）
 *
 * 用法：在 MT 管理器终端 / Termux 里以 root 运行（游戏先打开）：
 *   su
 *   chmod 755 gamemodifier
 *   ./gamemodifier
 *
 * 与 JNI 版的区别（都是上次分析里指出、且只存在于 C 逻辑里的问题，
 * gamemodifier.c 按要求未动，这里在新文件里按正确写法实现）：
 *   1. 指针按 64 位（uint64_t）读取，不再截断成 32 位
 *   2. 首偏移 0x56CFF0 加上 libil2cpp.so 模块基址后再解链
 *   3. 找进程改为精确匹配 cmdline 第一段，避免匹配到子进程
 * 其余流程（包名、模块名、指针链、写入值 110）与原版保持一致。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
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

/* 通过包名精确查找进程 PID（cmdline 第一段必须与包名完全相等） */
static pid_t FindProcessByPackage(const char *packageName)
{
    DIR *dir = opendir("/proc");
    if (!dir) return -1;

    pid_t result = -1;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (name[0] < '0' || name[0] > '9') continue;

        char path[256];
        snprintf(path, sizeof(path), "/proc/%s/cmdline", name);

        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;

        char cmdline[512];
        ssize_t len = read(fd, cmdline, sizeof(cmdline) - 1);
        close(fd);

        if (len <= 0) continue;
        cmdline[len] = '\0'; /* 第一段以 \0 结尾，strcmp 只比第一段 */

        if (strcmp(cmdline, packageName) == 0) {
            result = (pid_t)atoi(name);
            break;
        }
    }
    closedir(dir);
    return result;
}

/* 从 /proc/<pid>/maps 获取模块基址（该模块第一条映射的起始地址） */
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

/* 解析多级指针链：起点 = 模块基址 + 首偏移，指针按 64 位读取 */
static uintptr_t ResolvePointerChain(pid_t pid, uintptr_t moduleBase)
{
    uintptr_t current = moduleBase + OFFSETS[0];
    printf("  起点：基址 0x%" PRIxPTR " + 偏移 0x%" PRIxPTR " = 0x%" PRIxPTR "\n",
           moduleBase, OFFSETS[0], current);

    for (size_t i = 1; i < OFFSET_COUNT; i++) {
        uint64_t ptr = 0;
        if (ReadProcessMemory(pid, current, &ptr, sizeof(ptr)) != 0) {
            printf("  第 %zu 步读取指针失败，地址 0x%" PRIxPTR "，原因：%s\n",
                   i, current, strerror(errno));
            return 0;
        }
        printf("  第 %zu 步：[0x%" PRIxPTR "] = 0x%" PRIx64 "，+ 0x%" PRIxPTR "\n",
               i, current, ptr, OFFSETS[i]);
        if (ptr == 0) {
            printf("  第 %zu 步指针为空，指针链断了（偏移可能已失效）\n", i);
            return 0;
        }
        if (i == OFFSET_COUNT - 1) {
            return (uintptr_t)ptr + OFFSETS[i];
        }
        current = (uintptr_t)ptr + OFFSETS[i];
    }
    return 0;
}

int main(void)
{
    printf("GameModifier 终端版（arm64）\n");
    printf("目标包名：%s，模块：%s，写入值：%d\n", TARGET_PACKAGE, TARGET_MODULE, WRITE_VALUE);

    if (geteuid() != 0) {
        printf("警告：当前不是 root 运行，读取其他进程内存大概率会被拒绝。\n");
        printf("请先执行 su 提权后再运行本程序。\n");
    }

    pid_t pid = FindProcessByPackage(TARGET_PACKAGE);
    if (pid <= 0) {
        printf("失败：未找到游戏进程 %s，请先打开游戏再运行。\n", TARGET_PACKAGE);
        return 1;
    }
    printf("找到游戏进程，PID：%d\n", (int)pid);

    uintptr_t moduleBase = FindModuleBase(pid, TARGET_MODULE);
    if (moduleBase == 0) {
        printf("失败：在进程内存映射里未找到模块 %s。\n", TARGET_MODULE);
        return 1;
    }
    printf("模块基址：0x%" PRIxPTR "\n", moduleBase);

    printf("开始解析指针链：\n");
    uintptr_t finalAddr = ResolvePointerChain(pid, moduleBase);
    if (finalAddr == 0) {
        printf("失败：指针链解析失败。\n");
        return 1;
    }
    printf("最终地址：0x%" PRIxPTR "\n", finalAddr);

    uint32_t oldValue = 0;
    if (ReadProcessMemory(pid, finalAddr, &oldValue, sizeof(oldValue)) != 0) {
        printf("失败：读取当前值失败，原因：%s\n", strerror(errno));
        printf("（Permission denied 一般是没提权；I/O error 一般是地址不对）\n");
        return 1;
    }
    printf("当前值：%u\n", (unsigned int)oldValue);

    uint32_t newValue = WRITE_VALUE;
    if (WriteProcessMemory(pid, finalAddr, &newValue, sizeof(newValue)) != 0) {
        printf("失败：写入失败，原因：%s\n", strerror(errno));
        return 1;
    }

    uint32_t checkValue = 0;
    if (ReadProcessMemory(pid, finalAddr, &checkValue, sizeof(checkValue)) == 0) {
        printf("写入后读回：%u\n", (unsigned int)checkValue);
        if (checkValue != (uint32_t)WRITE_VALUE) {
            printf("失败：读回的值与写入值不一致（可能被游戏立即改回）。\n");
            return 1;
        }
    }
    printf("修改成功！新值：%d\n", WRITE_VALUE);
    return 0;
}
