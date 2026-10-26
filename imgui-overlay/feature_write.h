/*
 * feature_write.h - 动态修改功能（C 语言）
 *
 * 内容是用户提供的 GameGuardian Lua 脚本翻成的 C（对照表在 feature_write.c 里）：
 * 找和平精英（com.tencent.tmgp.pubgmhd）进程 -> 取 libUE4.so 的 bss 段地址
 * -> 按指针链 {0xB3D7F0, 0x0, 0x3C8, 0x2F4}（64 位）解析
 * -> 把最终地址的值按 float 写成用户在输入框里填的数值（默认 120）。
 */
#ifndef FEATURE_WRITE_H
#define FEATURE_WRITE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 动态写入一次（指针链）。value 是要写的 float 值。
 * 成功返回 0，失败返回非 0；message 里写入一句结果说明。 */
int Feature_ApplyWrite(float value, char *message, size_t messageSize);

#ifdef __cplusplus
}
#endif

#endif /* FEATURE_WRITE_H */
