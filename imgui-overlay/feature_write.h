/*
 * feature_write.h - 两个修改功能（C 语言）：动态修改 + 静态修改
 *
 * 动态修改：用户提供的 GameGuardian Lua 脚本（S_Pointer）翻成的 C——
 * 找游戏进程 -> 取 libUE4.so 的 bss 段地址 -> 按指针链（64 位）解析
 * -> 把最终地址的值按 float 写成用户在输入框里填的数值（默认 110.0）。
 *
 * 静态修改：用户提供的另一段 GG 写法翻成的 C——
 *   so = gg.getRangesList('libUE4.so')[1].start   （libUE4.so 第一段映射的起始地址）
 *   setvalue(so + 0x37C03B8, 16, 值)              （flags 16 = float）
 * 即地址 = libUE4.so 装载基址 + 0x37C03B8，写 float，点按钮写一次。
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

/* 静态写入一次（libUE4.so 基址 + 0x37C03B8）。value 是要写的 float 值。
 * 成功返回 0，失败返回非 0；message 里写入一句结果说明。 */
int Feature_ApplyStaticWrite(float value, char *message, size_t messageSize);

#ifdef __cplusplus
}
#endif

#endif /* FEATURE_WRITE_H */
