/*
 * feature_write.h - 复选框对应的实际功能（C 语言）
 *
 * 逻辑与终端版 modifier_main.c 相同：找游戏进程 -> 取 libil2cpp.so 基址
 * -> 按指针链解析 -> 把最终地址的值写成 110。区别是这里不 printf，
 * 而是把结果写进 message 返回给界面显示。
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
