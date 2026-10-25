/*
 * touch_input.h - 触摸输入（平台层，自己实现，不用库自带的 TouchHelperA）
 *
 * 原理：root 下直接读 /dev/input/event*（多点触控协议 B），
 * 把原始坐标按屏幕方向换算成 ImGui 坐标，再通过 ImGui 官方事件队列
 * （AddMousePosEvent / AddMouseButtonEvent）注入，必须在主线程、
 * 每帧 NewFrame 之前调用 Poll()。
 *
 * 为什么不用 TouchHelperA：它在触摸线程里直接写 io.MousePos/MouseDown，
 * 和 ImGui 1.92 的事件队列机制不合拍，还没有任何诊断输出，
 * 出了问题（坐标偏、点不动）完全看不出卡在哪一步。
 */
#pragma once

#include <cstddef>

namespace TouchInput {

/* longEdge/shortEdge：屏幕长边/短边像素（面板自然方向为竖屏：宽=短边，高=长边）。
 * 成功返回 true；找不到触摸设备或权限不足返回 false（原因会打印到终端）。 */
bool Init(int longEdge, int shortEdge);

/* 每帧把系统给的屏幕方向（0..3）告诉触摸模块，用于坐标换算 */
void SetOrientation(int orientation);

/* 主线程每帧调用（在 graphics->NewFrame() 之前）：把攒下的触摸事件喂给 ImGui */
void Poll();

void Shutdown();

bool IsReady();

/* 一行调试信息：触摸设备状态 + 最近一次触摸的屏幕坐标 + 按下/抬起，显示在窗体里 */
void GetDebugText(char *buffer, size_t size);

} /* namespace TouchInput */
