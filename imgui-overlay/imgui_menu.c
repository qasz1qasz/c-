/*
 * imgui_menu.c - ImGui 悬浮窗样式实现（C 语言，基于 cimgui）
 *
 * 样式要点：
 *   - 深色半透明窗体，圆角 8
 *   - 主色紫 #6C4DFF（复选框对勾、标题栏激活色都用它）
 *   - 窗口里只有一个功能：复选框「数值修改（写入 110）」
 *   - 复选框下面一行状态文字：已开启（紫）/ 已关闭（灰）
 *
 * 注意：界面里有中文，真正跑之前主程序要给 ImGui 加载中文字体
 * （系统字体 + 中文字形范围），否则中文会显示成方框。这一步在主程序里做。
 */

#include "imgui_menu.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS /* cimgui 要求：定义这个宏才会给出结构体和枚举定义 */
#include "cimgui.h"
#include <stddef.h> /* NULL */

void MenuStyle_Apply(void)
{
    ImGuiStyle *style = igGetStyle();
    igStyleColorsDark(style); /* 先套官方深色，再改下面几处 */

    /* 形状 */
    style->WindowRounding    = 8.0f;  /* 窗口圆角 */
    style->FrameRounding     = 4.0f;  /* 复选框圆角 */
    style->GrabRounding      = 4.0f;
    style->WindowBorderSize  = 1.0f;
    style->FrameBorderSize   = 0.0f;
    style->WindowPadding     = (ImVec2_c){ 14.0f, 12.0f };
    style->FramePadding      = (ImVec2_c){ 6.0f, 4.0f };
    style->ItemSpacing       = (ImVec2_c){ 8.0f, 10.0f };

    /* 颜色（RGBA，0~1） */
    ImVec4_c *colors = style->Colors;
    colors[ImGuiCol_WindowBg]     = (ImVec4_c){ 0.07f, 0.07f, 0.10f, 0.94f }; /* 窗体：近黑，略透 */
    colors[ImGuiCol_Border]       = (ImVec4_c){ 0.25f, 0.25f, 0.32f, 1.00f }; /* 边框：灰紫 */
    colors[ImGuiCol_TitleBg]      = (ImVec4_c){ 0.10f, 0.10f, 0.13f, 1.00f }; /* 标题栏 */
    colors[ImGuiCol_TitleBgActive]= (ImVec4_c){ 0.16f, 0.12f, 0.35f, 1.00f }; /* 标题栏激活：深紫 */
    colors[ImGuiCol_Text]         = (ImVec4_c){ 0.92f, 0.92f, 0.95f, 1.00f }; /* 主文字 */
    colors[ImGuiCol_TextDisabled] = (ImVec4_c){ 0.48f, 0.48f, 0.55f, 1.00f }; /* 次要文字 */
    colors[ImGuiCol_FrameBg]        = (ImVec4_c){ 0.14f, 0.14f, 0.18f, 1.00f }; /* 复选框底 */
    colors[ImGuiCol_FrameBgHovered] = (ImVec4_c){ 0.19f, 0.19f, 0.24f, 1.00f };
    colors[ImGuiCol_FrameBgActive]  = (ImVec4_c){ 0.23f, 0.20f, 0.45f, 1.00f };
    colors[ImGuiCol_CheckMark]    = (ImVec4_c){ 0.42f, 0.30f, 1.00f, 1.00f }; /* 对勾：主色紫 #6C4DFF */
    colors[ImGuiCol_Separator]    = (ImVec4_c){ 0.25f, 0.25f, 0.32f, 1.00f };
}

void Menu_Draw(MenuState *state)
{
    /* 首次出现的位置和宽度；高度自适应内容 */
    igSetNextWindowPos((ImVec2_c){ 60.0f, 180.0f }, ImGuiCond_FirstUseEver, (ImVec2_c){ 0.0f, 0.0f });
    igSetNextWindowSize((ImVec2_c){ 400.0f, 0.0f }, ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse
                           | ImGuiWindowFlags_AlwaysAutoResize;
    /* 传 &window_open 后标题栏会多一个 X，点它同样退出整个程序 */
    static bool window_open = true;
    bool visible = igBegin("Game Modifier", &window_open, flags);
    if (visible) {

        igTextColored((ImVec4_c){ 0.62f, 0.62f, 0.68f, 1.00f }, "功能");
        igSeparator();

        /* 唯一的功能：一个复选框。点动时通知主程序。 */
        if (igCheckbox("数值修改（写入 110.0）", &state->feature_on)) {
            OnFeatureToggled(state->feature_on ? 1 : 0);
        }

        igSpacing();

        /* 状态行 */
        if (state->feature_on) {
            igTextColored((ImVec4_c){ 0.42f, 0.30f, 1.00f, 1.00f }, "状态：已开启");
        } else {
            igTextColored((ImVec4_c){ 0.48f, 0.48f, 0.55f, 1.00f }, "状态：已关闭");
        }

        /* 主程序给的执行结果（成功/失败原因），有内容才显示 */
        if (state->status_text[0] != '\0') {
            igSpacing();
            igTextWrapped("%s", state->status_text);
        }

        igSpacing();

        /* 退出按钮：整行宽，暗红色，点了直接退出整个程序 */
        igPushStyleColor_Vec4(ImGuiCol_Button,        (ImVec4_c){ 0.40f, 0.13f, 0.15f, 1.00f });
        igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, (ImVec4_c){ 0.55f, 0.18f, 0.20f, 1.00f });
        igPushStyleColor_Vec4(ImGuiCol_ButtonActive,  (ImVec4_c){ 0.68f, 0.22f, 0.25f, 1.00f });
        if (igButton("退出程序", (ImVec2_c){ -1.0f, 0.0f })) {
            OnExitRequested();
        }
        igPopStyleColor(3);

        igSpacing();
        igTextDisabled("按住标题栏拖动 · 音量- 隐藏 · 音量+ 显示");

        /* 触摸调试行：坐标跟着触摸实时变，用来排查点不动 */
        if (state->debug_text[0] != '\0') {
            igTextDisabled("%s", state->debug_text);
        }
    }
    igEnd();

    /* 标题栏 X 被点掉时也退出整个程序 */
    if (!window_open) {
        OnExitRequested();
    }
}
