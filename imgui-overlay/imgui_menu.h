/*
 * imgui_menu.h - ImGui 悬浮窗（两个区域：动态修改 + 静态修改）
 *
 * 说明：Dear ImGui 本体是 C++ 库，这里用它的官方 C 绑定 cimgui，
 * 所以画界面的代码是纯 C 写的，函数都是 ig 开头。
 *
 * 输入说明：本程序是终端可执行文件、窗体是 SurfaceComposerClient 建的
 * 悬浮层，没有安卓窗口的输入通道，系统输入法（IME）唤不起来；
 * 所以输入框点开的是窗内自带的数字键盘（点按输入，和系统键盘同用途）。
 */
#ifndef IMGUI_MENU_H
#define IMGUI_MENU_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 悬浮窗的全部状态：两个区域各一份输入和状态 */
typedef struct {
    bool feature_on;     /* 动态区开关：开启后每秒按动态值持续写入（冻结） */
    float switch_anim;   /* 开关滑块动画进度 0..1（关..开），界面内部用 */
    char dynamic_input[32]; /* 动态区输入框文本（默认 "110"，主程序初始化） */
    char static_input[32];  /* 静态区输入框文本（默认 "2"，主程序初始化） */
    int focused_input;      /* 数字键盘正在编辑哪个输入框：0 无 / 1 动态 / 2 静态 */
    char status_text[128]; /* 主程序填的动态区最近一次执行结果，空字符串就不显示 */
    char static_status_text[128]; /* 主程序填的静态区最近一次执行结果 */
    char debug_text[128];  /* 主程序填的触摸调试信息，空字符串就不显示 */
} MenuState;

/* 动态开关被用户点动时回调到主程序（主程序里实现它，负责真正去写内存）。
 * enabled: 1 = 勾选（开启），0 = 取消勾选（关闭） */
void OnFeatureToggled(int enabled);

/* 动态区「确定修改」按钮：text 是动态输入框里的原文，
 * 主程序解析成 float、立即按动态指针链写一次，并把这个值定为之后
 * 开关冻结时每秒重写的值 */
void OnDynamicConfirm(const char *text);

/* 静态区「确定修改」按钮：text 是静态输入框里的原文，
 * 主程序解析成 float、立即按静态地址（libUE4.so 基址 + 0x37C03B8）写一次 */
void OnStaticConfirm(const char *text);

/* 用户点「退出程序」按钮或标题栏的 X 时回调到主程序：
 * 主程序停主循环、关悬浮层、结束整个进程 */
void OnExitRequested(void);

/* 应用样式（颜色、圆角、间距）。在 igCreateContext 之后调用一次。 */
void MenuStyle_Apply(void);

/* 每帧调用一次，画出悬浮窗。state 里保存界面当前状态。 */
void Menu_Draw(MenuState *state);

#ifdef __cplusplus
}
#endif

#endif /* IMGUI_MENU_H */
