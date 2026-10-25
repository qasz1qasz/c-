/*
 * volume_keys.h - 音量键监听（平台层）
 *
 * 音量键和触摸一样是 /dev/input 的输入事件，只是来自另一个设备、
 * 类型是 EV_KEY（KEY_VOLUMEUP=115 / KEY_VOLUMEDOWN=114）。
 * 这里 root 下只读旁听：不拦截，系统音量该变还是变，我们只是顺带
 * 拿它当悬浮窗的显示/隐藏开关。
 *
 * 用法：Init() 启动监听线程；主线程每帧调 ConsumeShowRequest /
 * ConsumeHideRequest 取走「按过」的标记。
 */
#pragma once

namespace VolumeKeys {

/* 扫描带音量键的输入设备并开始监听。找不到设备或无权打开返回 false（原因打印到终端）。 */
bool Init();

void Shutdown();

/* 自上次调用以来按过音量+（按下瞬间，长按连发不算）返回 true，并清掉标记 */
bool ConsumeShowRequest();

/* 自上次调用以来按过音量- 返回 true，并清掉标记 */
bool ConsumeHideRequest();

} /* namespace VolumeKeys */
