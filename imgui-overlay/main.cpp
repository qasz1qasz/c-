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
 * 退出：点窗体里的「退出程序」按钮或标题栏 X，或在终端按 Ctrl+C。
 */

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
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
#include "volume_keys.h"

extern "C" {
#include "imgui_menu.h"
#include "feature_write.h"
}

static MenuState g_menu{};                 /* 界面状态（主线程读写） */
static bool g_windowVisible = true;        /* 窗体显示开关：音量+ 显示、音量- 隐藏 */
static std::mutex g_statusMutex;           /* 保护 g_lastResult */
static std::string g_lastResult;           /* 后台线程写、主线程读的执行结果 */
static volatile std::sig_atomic_t g_running = 1;

static void OnSignal(int) { g_running = 0; }

/* 动态开关被点动时由界面回调：开启 -> 启动/继续写入；关闭 -> 停 */
static std::mutex g_featureMutex;
static bool g_featureEnabled = false;
static float g_dynamicValue = 110.0f;   /* 动态区当前数值（输入框确定后更新） */

/* 静态区执行结果（只有主线程读写，仍用锁和动态那边保持一致写法） */
static std::mutex g_staticMutex;
static std::string g_staticResult;

extern "C" void OnFeatureToggled(int enabled)
{
    std::lock_guard<std::mutex> lock(g_featureMutex);
    g_featureEnabled = (enabled != 0);
    if (!enabled) {
        std::lock_guard<std::mutex> lock2(g_statusMutex);
        g_lastResult.clear();
    }
}

/* 把输入框文本解析成 float；有杂字符、空串都算无效 */
static bool ParseFloatValue(const char *text, float *out)
{
    if (text == nullptr || *text == '\0') return false;
    char *end = nullptr;
    float value = std::strtof(text, &end);
    if (end == text) return false;
    while (*end != '\0' && std::isspace((unsigned char)*end)) end++;
    if (*end != '\0') return false;
    *out = value;
    return true;
}

/* 动态区「确定修改」：定下新数值并立即写一次；
 * 开关开着时，后台线程之后每秒也按这个新数值重写 */
extern "C" void OnDynamicConfirm(const char *text)
{
    float value;
    if (!ParseFloatValue(text, &value)) {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        g_lastResult = "失败：动态数值无效，点输入框用键盘输入数字";
        return;
    }
    {
        std::lock_guard<std::mutex> lock(g_featureMutex);
        g_dynamicValue = value;
    }
    char message[128];
    Feature_ApplyWrite(value, message, sizeof(message));
    std::lock_guard<std::mutex> lock(g_statusMutex);
    g_lastResult = message;
}

/* 静态区「确定修改」：按静态地址（libUE4.so 基址 + 0x37C03B8）立即写一次 */
extern "C" void OnStaticConfirm(const char *text)
{
    float value;
    if (!ParseFloatValue(text, &value)) {
        std::lock_guard<std::mutex> lock(g_staticMutex);
        g_staticResult = "失败：静态数值无效，点输入框用键盘输入数字";
        return;
    }
    char message[128];
    Feature_ApplyStaticWrite(value, message, sizeof(message));
    std::lock_guard<std::mutex> lock(g_staticMutex);
    g_staticResult = message;
}

/* 窗体里点「退出程序」按钮或标题栏 X：停主循环，后面统一走清理退出 */
extern "C" void OnExitRequested()
{
    g_running = 0;
}

/* 后台线程：动态开关开启期间每秒按当前动态数值执行一次写入，
 * 把结果存下来给界面显示（静态修改不在这里，点按钮才写） */
static void FeatureWorker()
{
    while (g_running) {
        bool enabled;
        float value;
        {
            std::lock_guard<std::mutex> lock(g_featureMutex);
            enabled = g_featureEnabled;
            value = g_dynamicValue;
        }
        if (enabled) {
            char message[128];
            Feature_ApplyWrite(value, message, sizeof(message));
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

    /* 0. 系统接口自检：平台层靠运行时解析 libgui 的私有符号建窗，
     * 某个符号在本机系统上不存在时指针就是空的，旧版直接调用会段错误、什么都看不到。
     * 这里先逐个验一遍，缺了就把名字打印出来再退出，方便对着系统版本适配。 */
    {
        const auto &funcs = android::detail::Functionals::GetInstance();
        printf("系统版本：Android %zu\n", funcs.systemVersion);
        struct SymbolCheck { const char *name; const void *ptr; };
        const SymbolCheck checks[] = {
            {"RefBase::incStrong",     reinterpret_cast<const void *>(funcs.RefBase__IncStrong)},
            {"RefBase::decStrong",     reinterpret_cast<const void *>(funcs.RefBase__DecStrong)},
            {"String8 构造函数",        reinterpret_cast<const void *>(funcs.String8__Constructor)},
            {"String8 析构函数",        reinterpret_cast<const void *>(funcs.String8__Destructor)},
            {"LayerMetadata 构造函数",  reinterpret_cast<const void *>(funcs.LayerMetadata__Constructor)},
            {"SurfaceComposerClient 构造函数", reinterpret_cast<const void *>(funcs.SurfaceComposerClient__Constructor)},
            {"SurfaceComposerClient::createSurface", reinterpret_cast<const void *>(funcs.SurfaceComposerClient__CreateSurface)},
            {"getPhysicalDisplayIds",  reinterpret_cast<const void *>(funcs.SurfaceComposerClient__GetPhysicalDisplayIds)},
            {"getPhysicalDisplayToken", reinterpret_cast<const void *>(funcs.SurfaceComposerClient__GetPhysicalDisplayToken)},
            {"getDisplayState",        reinterpret_cast<const void *>(funcs.SurfaceComposerClient__GetDisplayState)},
            {"Transaction 构造函数",    reinterpret_cast<const void *>(funcs.SurfaceComposerClient__Transaction__Constructor)},
            {"Transaction::setTrustedOverlay", reinterpret_cast<const void *>(funcs.SurfaceComposerClient__Transaction__SetTrustedOverlay)},
            {"Transaction::apply",     reinterpret_cast<const void *>(funcs.SurfaceComposerClient__Transaction__Apply)},
            {"SurfaceControl::validate", reinterpret_cast<const void *>(funcs.SurfaceControl__Validate)},
            {"SurfaceControl::getSurface", reinterpret_cast<const void *>(funcs.SurfaceControl__GetSurface)},
            {"SurfaceControl::disconnect", reinterpret_cast<const void *>(funcs.SurfaceControl__DisConnect)},
        };
        int missing = 0;
        for (const auto &c : checks) {
            if (c.ptr == nullptr) {
                printf("缺失系统接口：%s\n", c.name);
                missing++;
            }
        }
        if (missing > 0) {
            printf("失败：%d 个系统接口在本机系统上找不到，悬浮层建不出来（缺失名单见上）。\n", missing);
            return 1;
        }
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

    /* 5. 音量键（同样只读旁听 /dev/input）：音量+ 显示窗体、音量- 隐藏窗体 */
    if (!VolumeKeys::Init()) {
        printf("警告：音量键监听没起来，窗体只能一直显示（原因见上面一行）。\n");
    }

    /* 输入框默认值：动态 110（原 Lua 的值）、静态 2（静态广角的值） */
    std::snprintf(g_menu.dynamic_input, sizeof(g_menu.dynamic_input), "110");
    std::snprintf(g_menu.static_input, sizeof(g_menu.static_input), "2");

    std::thread worker(FeatureWorker);

    /* 6. 主循环 */
    printf("悬浮窗已启动。音量+ 显示窗体，音量- 隐藏窗体；点窗内「退出程序」或 Ctrl+C 退出。\n");
    while (g_running) {
        /* 音量键开关窗体（隐藏只是不画窗，功能开关状态不变） */
        if (VolumeKeys::ConsumeShowRequest()) g_windowVisible = true;
        if (VolumeKeys::ConsumeHideRequest()) g_windowVisible = false;

        /* 触摸事件必须赶在 NewFrame 之前喂进 ImGui 的事件队列 */
        TouchInput::SetOrientation(
                (int)android::ANativeWindowCreator::GetDisplayInfo().orientation);
        TouchInput::Poll();

        graphics->NewFrame();

        if (g_windowVisible) {
            /* 先把结果拷进界面状态再画：画的过程中按钮回调要写这些结果，
             * 不能画着还攥着锁（回调里再抢同一把锁会死锁） */
            {
                std::lock_guard<std::mutex> lock(g_statusMutex);
                std::snprintf(g_menu.status_text, sizeof(g_menu.status_text),
                              "%s", g_lastResult.c_str());
            }
            {
                std::lock_guard<std::mutex> lock(g_staticMutex);
                std::snprintf(g_menu.static_status_text,
                              sizeof(g_menu.static_status_text),
                              "%s", g_staticResult.c_str());
            }
            TouchInput::GetDebugText(g_menu.debug_text, sizeof(g_menu.debug_text));
            Menu_Draw(&g_menu);
        }

        graphics->EndFrame();
    }

    g_running = 0;
    if (worker.joinable()) worker.join();
    VolumeKeys::Shutdown();
    TouchInput::Shutdown();
    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(window);
    printf("已退出。\n");
    return 0;
}
