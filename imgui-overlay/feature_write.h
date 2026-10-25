/*
 * feature_write.h - 复选框对应的实际功能（C 语言）
 *
 * 内容是用户提供的 GameGuardian Lua 脚本翻成的 C（对照表在 feature_write.c 里）：
 * 找游戏进程 -> 取 libUE4.so 的 bss 段地址 -> 按指针链（64 位）解析
 * -> 把最终地址的值按 float 写成 110.0。结果写进 message 返回给界面显示。
 */
#ifndef FEATURE_WRITE_H
#define FEATURE_WRITE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 执行一次写入。成功返回 0，失败返回非 0；message 里写入一句结果说明。 */
int Feature_ApplyWrite(char *message, size_t messageSize);

#ifdef __cplusplus
}
#endif

#endif /* FEATURE_WRITE_H */
