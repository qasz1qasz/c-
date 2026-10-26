/*
 * GameModifier - 原生内存修改库
 * 目标包名：com.rekoo.pubgm
 * 指针链：0x56CFF0 -> 0x30 -> 0x4B8 -> 0x360 -> 0x170 -> 0x39C
 * 修改值：110
 */

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <dirent.h>
#include <android/log.h>

#define LOG_TAG "GameModifier"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define TARGET_PACKAGE "com.tencent.tmgp.pubgmhd"
#define TARGET_MODULE  "libil2cpp.so"

static const uintptr_t OFFSETS[] = {
    0xB3D7F0, 0x0, 0x3C8, 0x2F4
};
#define OFFSET_COUNT (sizeof(OFFSETS) / sizeof(OFFSETS[0]))

/* ================= 防封函数 ================= */
/* 防封逻辑由你自己实现，这里只保留接口 */
void AntiBanProtect(pid_t pid)
{
    (void)pid;
    LOGI("AntiBanProtect 已调用，等待用户实现具体逻辑");
}

/* ================= 工具函数 ================= */

/* 通过包名查找进程 PID */
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
        cmdline[len] = '\0';

        if (strstr(cmdline, packageName) != NULL) {
            result = (pid_t)atoi(name);
            break;
        }
    }
    closedir(dir);
    return result;
}

/* 从 /proc/<pid>/maps 获取模块基址 */
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

        unsigned long start = 0, end = 0;
        if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
            base = (uintptr_t)start;
            break;
        }
    }
    fclose(fp);
    return base;
}

/* 读取目标进程内存 */
static int ReadProcessMemory(pid_t pid, uintptr_t address, void *buffer, size_t size)
{
    char memPath[256];
    snprintf(memPath, sizeof(memPath), "/proc/%d/mem", (int)pid);

    int fd = open(memPath, O_RDONLY);
    if (fd < 0) return -1;

    ssize_t ret = pread(fd, buffer, size, (off_t)address);
    close(fd);
    return (ret == (ssize_t)size) ? 0 : -1;
}

/* 写入目标进程内存 */
static int WriteProcessMemory(pid_t pid, uintptr_t address, const void *buffer, size_t size)
{
    char memPath[256];
    snprintf(memPath, sizeof(memPath), "/proc/%d/mem", (int)pid);

    int fd = open(memPath, O_RDWR);
    if (fd < 0) return -1;

    ssize_t ret = pwrite(fd, buffer, size, (off_t)address);
    close(fd);
    return (ret == (ssize_t)size) ? 0 : -1;
}

/* 解析多级指针链（0x56CFF0 按绝对地址处理） */
static uintptr_t ResolvePointerChain(pid_t pid, const uintptr_t *offsets, size_t count)
{
    if (!offsets || count == 0) return 0;

    uintptr_t current = offsets[0];
    uint32_t nextPtr = 0;

    for (size_t i = 1; i < count - 1; i++) {
        if (ReadProcessMemory(pid, current, &nextPtr, sizeof(uint32_t)) != 0) {
            LOGE("读取指针失败，索引：%zu，地址：0x%lx", i, (unsigned long)current);
            return 0;
        }
        current = (uintptr_t)nextPtr + offsets[i];
    }

    if (ReadProcessMemory(pid, current, &nextPtr, sizeof(uint32_t)) != 0) {
        LOGE("读取最后一级指针失败，地址：0x%lx", (unsigned long)current);
        return 0;
    }
    return (uintptr_t)nextPtr + offsets[count - 1];
}

/* ================= JNI 接口 ================= */

/* JNI: 查找游戏进程，返回 PID */
JNIEXPORT jint JNICALL
Java_com_gamemod_NativeLib_findGameProcess(JNIEnv *env, jobject thiz)
{
    (void)env; (void)thiz;
    pid_t pid = FindProcessByPackage(TARGET_PACKAGE);
    if (pid > 0) {
        LOGI("找到游戏进程，PID：%d", (int)pid);
    } else {
        LOGE("未找到进程：%s", TARGET_PACKAGE);
    }
    return (jint)pid;
}

/* JNI: 读取指针链指向的值，返回 int 数组 [PID, 最终地址, 当前值] */
JNIEXPORT jintArray JNICALL
Java_com_gamemod_NativeLib_readValue(JNIEnv *env, jobject thiz)
{
    (void)thiz;
    jintArray result = (*env)->NewIntArray(env, 3);
    jint buf[3] = {0, 0, 0};

    pid_t pid = FindProcessByPackage(TARGET_PACKAGE);
    if (pid <= 0) {
        (*env)->SetIntArrayRegion(env, result, 0, 3, buf);
        return result;
    }
    buf[0] = (jint)pid;

    uintptr_t finalAddr = ResolvePointerChain(pid, OFFSETS, OFFSET_COUNT);
    if (finalAddr == 0) {
        (*env)->SetIntArrayRegion(env, result, 0, 3, buf);
        return result;
    }
    buf[1] = (jint)finalAddr;

    uint32_t value = 0;
    if (ReadProcessMemory(pid, finalAddr, &value, sizeof(uint32_t)) == 0) {
        buf[2] = (jint)value;
    }

    (*env)->SetIntArrayRegion(env, result, 0, 3, buf);
    return result;
}

/* JNI: 写入新值（固定值110），enableAntiBan=1 时调用防封函数 */
JNIEXPORT jboolean JNICALL
Java_com_gamemod_NativeLib_writeValue(JNIEnv *env, jobject thiz, jboolean enableAntiBan)
{
    (void)env; (void)thiz;
    pid_t pid = FindProcessByPackage(TARGET_PACKAGE);
    if (pid <= 0) return JNI_FALSE;

    uintptr_t finalAddr = ResolvePointerChain(pid, OFFSETS, OFFSET_COUNT);
    if (finalAddr == 0) return JNI_FALSE;

    if (enableAntiBan == JNI_TRUE) {
        AntiBanProtect(pid);
    }

    uint32_t val = 110;
    int ret = WriteProcessMemory(pid, finalAddr, &val, sizeof(uint32_t));
    if (ret == 0) {
        LOGI("写入成功，新值：%u，地址：0x%lx", (unsigned int)val, (unsigned long)finalAddr);
        return JNI_TRUE;
    }
    LOGE("写入失败");
    return JNI_FALSE;
}

/* JNI: 获取模块基址（调试用） */
JNIEXPORT jlong JNICALL
Java_com_gamemod_NativeLib_getModuleBase(JNIEnv *env, jobject thiz)
{
    (void)env; (void)thiz;
    pid_t pid = FindProcessByPackage(TARGET_PACKAGE);
    if (pid <= 0) return 0;
    return (jlong)FindModuleBase(pid, TARGET_MODULE);
}

/* JNI: 获取目标包名字符串 */
JNIEXPORT jstring JNICALL
Java_com_gamemod_NativeLib_getTargetPackage(JNIEnv *env, jobject thiz)
{
    (void)thiz;
    return (*env)->NewStringUTF(env, TARGET_PACKAGE);
}
