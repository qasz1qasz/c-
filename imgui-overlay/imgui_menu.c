/*
 * imgui_menu.c - ImGui 悬浮窗样式实现（C 语言，基于 cimgui）
 *
 * 样式要点：
 *   - 深色半透明窗体，圆角 8
 *   - 主色紫 #6C4DFF（开关打开色、标题栏激活色都用它）
 *   - 窗口里只有一个功能：滑动开关「数值修改（写入 110.0）」（ImGui 没有现成
 *     的开关控件，用 InvisibleButton + DrawList 自绘，滑块带滑动动画）
 *   - 开关下面一行状态文字：已开启（紫）/ 已关闭（灰）
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

/* 把 0~1 的 RGBA 浮点色打包成 ImU32（DrawList 要的颜色格式） */
static ImU32 PackColor(float r, float g, float b, float a)
{
    return ((ImU32)(a * 255.0f) << 24)
         | ((ImU32)(b * 255.0f) << 16)
         | ((ImU32)(g * 255.0f) << 8)
         |  (ImU32)(r * 255.0f);
}

/*
 * 自绘滑动开关（ImGui 没有现成的开关控件）：
 * InvisibleButton 负责点击判定，轨道和滑块用 DrawList 画，
 * 滑块位置按 switch_anim 在关/开之间滑动。被点动返回 true。
 */
static bool DrawFeatureSwitch(MenuState *state)
{
    float height = igGetFrameHeight();
    float width  = height * 1.9f;
    float radius = height * 0.5f;

    ImVec2_c pos = igGetCursorScreenPos();
    bool pressed = igInvisibleButton("##feature_switch",
                                     (ImVec2_c){ width, height }, 0);
    bool changed = false;
    if (pressed) {
        state->feature_on = !state->feature_on;
        changed = true;
    }

    /* 动画：switch_anim 向目标（开=1 / 关=0）靠拢 */
    float target = state->feature_on ? 1.0f : 0.0f;
    float step = igGetIO_Nil()->DeltaTime * 12.0f; /* cimgui 里 GetIO 叫 igGetIO_Nil */
    if (step > 1.0f) step = 1.0f;
    state->switch_anim += (target - state->switch_anim) * step;
    if (state->switch_anim < 0.0f) state->switch_anim = 0.0f;
    if (state->switch_anim > 1.0f) state->switch_anim = 1.0f;
    float t = state->switch_anim;

    /* 轨道色：关（灰）-> 开（主色紫 #6C4DFF），按动画进度插值 */
    ImU32 trackColor = PackColor(0.24f + (0.42f - 0.24f) * t,
                                 0.24f + (0.30f - 0.24f) * t,
                                 0.29f + (1.00f - 0.29f) * t, 1.0f);
    ImU32 knobColor  = PackColor(0.95f, 0.95f, 0.97f, 1.0f);

    ImDrawList *draw = igGetWindowDrawList();
    ImDrawList_AddRectFilled(draw,
                             (ImVec2_c){ pos.x, pos.y },
                             (ImVec2_c){ pos.x + width, pos.y + height },
                             trackColor, radius, 0);
    float knobX = pos.x + radius + t * (width - height);
    ImDrawList_AddCircleFilled(draw,
                               (ImVec2_c){ knobX, pos.y + radius },
                               radius - 3.0f, knobColor, 32);

    igSameLine(0.0f, 10.0f);
    igTextUnformatted("数值修改（写入 110.0）", NULL);
    return changed;
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

        /* 唯一的功能：一个滑动开关。点动时通知主程序。 */
        if (DrawFeatureSwitch(state)) {
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
