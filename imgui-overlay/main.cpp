/*
 * main.cpp - ImGui 悬浮窗终端版主程序（平台层，C++）
 *
 * 界面和功能都是 C 写的（imgui_menu.c / feature_write.c），
 * 这里只负责安卓平台层：建全屏透明 Surface、OpenGL ES 渲染、读触摸。
 * 平台层用开源库 AndroidImgui（enenH/AndroidImgui v2.1）：
 * 它通过系统 libgui 的 SurfaceComposerClient 创建悬浮层。
 * 触摸是我们自己写的 touch_input.cpp：直接读 /dev/input，
 * 经 ImGui 事件队列注入（库自带的 TouchHelperA 实测点不动，已弃用）。
 *
 * 用法（root 终端）：
 *   chmod 755 gamemodifier_imgui
 *   su
 *   ./gamemodifier_imgui
 * 退出：在终端按 Ctrl+C。
 */

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>

#include "ANativeWindowCreator.h"
#include "GraphicsManager.h"
#include "my_imgui.h"
#include "touch_input.h"

extern "C" {
#include "imgui_menu.h"
#include "feature_write.h"
}

static MenuState g_menu{};                 /* 界面状态（主线程读写） */
static std::mutex g_statusMutex;           /* 保护 g_lastResult */
static std::string g_lastResult;           /* 后台线程写、主线程读的执行结果 */
static volatile std::sig_atomic_t g_running = 1;

static void OnSignal(int) { g_running = 0; }

/* 复选框被点动时由界面回调：开启 -> 启动/继续写入；关闭 -> 停 */
static std::mutex g_featureMutex;
static bool g_featureEnabled = false;

extern "C" void OnFeatureToggled(int enabled)
{
    std::lock_guard<std::mutex> lock(g_featureMutex);
    g_featureEnabled = (enabled != 0);
    if (!enabled) {
        std::lock_guard<std::mutex> lock2(g_statusMutex);
        g_lastResult.clear();
    }
}

/* 后台线程：功能开启期间每秒执行一次写入，把结果存下来给界面显示 */
static void FeatureWorker()
{
    while (g_running) {
        bool enabled;
        {
            std::lock_guard<std::mutex> lock(g_featureMutex);
            enabled = g_featureEnabled;
        }
        if (enabled) {
            char message[128];
            Feature_ApplyWrite(message, sizeof(message));
            std::lock_guard<std::mutex> lock(g_statusMutex);
            g_lastResult = message;
        }
        usleep(200 * 1000);
        if (enabled) usleep(800 * 1000);
    }
}

int main()
{
    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    printf("GameModifier ImGui 悬浮窗（arm64）\n");
    if (geteuid() != 0) {
        printf("警告：当前不是 root。创建悬浮层、读触摸、写游戏内存都需要 root，\n");
        printf("请先 su 再运行，否则大概率失败。\n");
    }

    /* 1. 拿屏幕信息并创建全屏悬浮层（长边为宽，方形画布覆盖旋转前后的屏幕） */
    auto display = android::ANativeWindowCreator::GetDisplayInfo();
    if (display.width <= 0 || display.height <= 0) {
        printf("失败：拿不到屏幕信息（系统版本不受支持或权限不足）。\n");
        return 1;
    }
    if (display.height > display.width) {
        std::swap(display.height, display.width);
    }
    printf("屏幕：%d x %d，方向 %u\n", display.width, display.height, display.orientation);

    ANativeWindow *window = android::ANativeWindowCreator::Create(
            "GameModifier", display.width, display.width);
    if (window == nullptr) {
        printf("失败：悬浮层创建失败。\n");
        return 1;
    }

    /* 2. OpenGL ES 渲染后端 + ImGui 上下文 */
    auto graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::OPENGL);
    if (!graphics || !graphics->Init(window, (float)display.width, (float)display.width)) {
        printf("失败：图形初始化失败。\n");
        android::ANativeWindowCreator::Destroy(window);
        return 1;
    }

    /* 3. 中文字体 + 我们的样式（顺序不能反：先建上下文，再套样式） */
    if (!ImGui::Android_LoadSystemFont(30.0f)) {
        printf("警告：系统 CJK 字体没找到，中文可能显示成方框。\n");
    }
    MenuStyle_Apply();

    /* 4. 触摸（自己读 /dev/input，只读旁路：不拦截游戏操作） */
    if (!TouchInput::Init(display.width, display.height)) {
        printf("警告：触摸初始化失败，悬浮窗能显示但点不动（原因见上面一行）。\n");
    }

    std::thread worker(FeatureWorker);

    /* 5. 主循环 */
    printf("悬浮窗已启动。关闭终端或按 Ctrl+C 退出。\n");
    while (g_running) {
        /* 触摸事件必须赶在 NewFrame 之前喂进 ImGui 的事件队列 */
        TouchInput::SetOrientation(
                (int)android::ANativeWindowCreator::GetDisplayInfo().orientation);
        TouchInput::Poll();

        graphics->NewFrame();

        {
            std::lock_guard<std::mutex> lock(g_statusMutex);
            std::snprintf(g_menu.status_text, sizeof(g_menu.status_text),
                          "%s", g_lastResult.c_str());
        }
        TouchInput::GetDebugText(g_menu.debug_text, sizeof(g_menu.debug_text));
        Menu_Draw(&g_menu);

        graphics->EndFrame();
    }

    g_running = 0;
    if (worker.joinable()) worker.join();
    TouchInput::Shutdown();
    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(window);
    printf("已退出。\n");
    return 0;
}
