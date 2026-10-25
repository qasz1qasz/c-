/*
 * imgui_menu.h - ImGui 悬浮窗（样式版，只有 1 个功能：一个复选框）
 *
 * 说明：Dear ImGui 本体是 C++ 库，这里用它的官方 C 绑定 cimgui，
 * 所以画界面的代码是纯 C 写的，函数都是 ig 开头。
 */
#ifndef IMGUI_MENU_H
#define IMGUI_MENU_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 悬浮窗的全部状态：目前只有一个功能开关 */
typedef struct {
    bool feature_on;     /* 复选框：数值修改（写入 110） */
    char status_text[128]; /* 主程序填的最近一次执行结果，空字符串就不显示 */
    char debug_text[128];  /* 主程序填的触摸调试信息，空字符串就不显示 */
} MenuState;

/* 功能开关被用户点动时回调到主程序（主程序里实现它，负责真正去写内存）。
 * enabled: 1 = 勾选（开启），0 = 取消勾选（关闭） */
void OnFeatureToggled(int enabled);

/* 应用样式（颜色、圆角、间距）。在 igCreateContext 之后调用一次。 */
void MenuStyle_Apply(void);

/* 每帧调用一次，画出悬浮窗。state 里保存复选框当前状态。 */
void Menu_Draw(MenuState *state);

#ifdef __cplusplus
}
#endif

#endif /* IMGUI_MENU_H */
