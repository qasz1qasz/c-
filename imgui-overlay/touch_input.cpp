/*
 * touch_input.cpp - 见 touch_input.h 的说明。
 *
 * 坐标换算（面板自然方向 = 竖屏，u 为横坐标 0..natW，v 为纵坐标 0..natH）：
 *   方向 0: x = u,          y = v
 *   方向 1: x = v,          y = natW - u
 *   方向 2: x = natW - u,   y = natH - v
 *   方向 3: x = natH - v,   y = u
 * 和 Android InputReader 的旋转公式一致。
 */

#include "touch_input.h"

#include "imgui.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

namespace TouchInput {

static std::atomic<bool> g_running{false};
static std::atomic<int> g_orientation{0};
static std::thread g_thread;
static int g_fd = -1;
static int g_minX = 0, g_maxX = 0, g_minY = 0, g_maxY = 0;
static int g_natW = 0, g_natH = 0; /* 面板自然方向像素：宽=短边，高=长边 */
static bool g_ready = false;
static char g_devicePath[64] = "";

struct Shared {
    std::mutex mutex;
    bool hasPos = false;
    float x = 0.0f, y = 0.0f;
    bool down = false;
    bool posChanged = false;
    bool downChanged = false;
    long touchCount = 0; /* 累计按下次数，方便确认事件有没有进来 */
};
static Shared g_shared;

struct SlotState {
    bool active = false;
    bool hasPos = false;
    int rawX = 0, rawY = 0;
};

static float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

static void ReaderThread()
{
    SlotState slots[10];
    int currentSlot = 0;
    bool wasDown = false;
    float lastX = 0.0f, lastY = 0.0f;

    struct pollfd pfd{};
    pfd.fd = g_fd;
    pfd.events = POLLIN;

    while (g_running.load()) {
        int ret = poll(&pfd, 1, 100);
        if (ret <= 0 || !(pfd.revents & POLLIN)) continue;

        struct input_event events[64];
        ssize_t n = read(g_fd, events, sizeof(events));
        if (n <= 0) continue;
        int count = (int)(n / (ssize_t)sizeof(struct input_event));

        for (int i = 0; i < count; i++) {
            const struct input_event &e = events[i];
            if (e.type == EV_ABS) {
                if (e.code == ABS_MT_SLOT) {
                    if (e.value >= 0 && e.value < 10) currentSlot = e.value;
                } else if (e.code == ABS_MT_TRACKING_ID) {
                    slots[currentSlot].active = (e.value >= 0);
                    if (!slots[currentSlot].active) slots[currentSlot].hasPos = false;
                } else if (e.code == ABS_MT_POSITION_X) {
                    slots[currentSlot].rawX = e.value;
                    slots[currentSlot].hasPos = true;
                } else if (e.code == ABS_MT_POSITION_Y) {
                    slots[currentSlot].rawY = e.value;
                    slots[currentSlot].hasPos = true;
                }
            } else if (e.type == EV_SYN && e.code == SYN_REPORT) {
                /* 取第一个正在按下的手指 */
                const SlotState *finger = nullptr;
                for (const auto &s : slots) {
                    if (s.active && s.hasPos) { finger = &s; break; }
                }
                std::lock_guard<std::mutex> lock(g_shared.mutex);
                if (finger) {
                    float nx = Clamp01((float)(finger->rawX - g_minX) /
                                       (float)((g_maxX - g_minX) > 0 ? (g_maxX - g_minX) : 1));
                    float ny = Clamp01((float)(finger->rawY - g_minY) /
                                       (float)((g_maxY - g_minY) > 0 ? (g_maxY - g_minY) : 1));
                    float u = nx * (float)g_natW;
                    float v = ny * (float)g_natH;
                    float x, y;
                    switch (g_orientation.load()) {
                        case 1:  x = v;            y = (float)g_natW - u; break;
                        case 2:  x = (float)g_natW - u; y = (float)g_natH - v; break;
                        case 3:  x = (float)g_natH - v; y = u;            break;
                        default: x = u;            y = v;            break;
                    }
                    if (!g_shared.down || x != g_shared.x || y != g_shared.y)
                        g_shared.posChanged = true;
                    g_shared.x = lastX = x;
                    g_shared.y = lastY = y;
                    g_shared.hasPos = true;
                    if (!wasDown) {
                        g_shared.downChanged = true;
                        g_shared.touchCount++;
                        wasDown = true;
                    }
                    g_shared.down = true;
                } else if (wasDown) {
                    /* 手指全抬起：位置保持最后一次，只报抬起 */
                    g_shared.x = lastX;
                    g_shared.y = lastY;
                    g_shared.down = false;
                    g_shared.downChanged = true;
                    wasDown = false;
                }
            }
        }
    }
}

bool Init(int longEdge, int shortEdge)
{
    Shutdown();
    g_natW = shortEdge;
    g_natH = longEdge;

    DIR *dir = opendir("/dev/input");
    if (!dir) {
        printf("触摸：打不开 /dev/input（权限不足？请用 root 运行）。\n");
        return false;
    }
    closedir(dir);

    char path[64];
    for (int i = 0; i < 32; i++) {
        std::snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;
        struct input_absinfo absX{}, absY{};
        bool ok = ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &absX) == 0 &&
                  ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &absY) == 0 &&
                  absX.maximum > absX.minimum && absY.maximum > absY.minimum;
        if (ok) {
            g_fd = fd;
            g_minX = absX.minimum; g_maxX = absX.maximum;
            g_minY = absY.minimum; g_maxY = absY.maximum;
            std::snprintf(g_devicePath, sizeof(g_devicePath), "%s", path);
            printf("触摸：设备 %s，原始范围 x %d..%d，y %d..%d\n",
                   path, g_minX, g_maxX, g_minY, g_maxY);
            break;
        }
        close(fd);
    }
    if (g_fd < 0) {
        printf("触摸：没找到触摸设备（/dev/input 下没有多点触控设备，或无权打开）。\n");
        return false;
    }

    g_ready = true;
    g_running.store(true);
    g_thread = std::thread(ReaderThread);
    return true;
}

void SetOrientation(int orientation)
{
    g_orientation.store(orientation);
}

void Poll()
{
    if (!g_ready || ImGui::GetCurrentContext() == nullptr) return;

    float x, y;
    bool down, posChanged, downChanged, hasPos;
    {
        std::lock_guard<std::mutex> lock(g_shared.mutex);
        x = g_shared.x; y = g_shared.y;
        down = g_shared.down;
        hasPos = g_shared.hasPos;
        posChanged = g_shared.posChanged;
        downChanged = g_shared.downChanged;
        g_shared.posChanged = false;
        g_shared.downChanged = false;
    }
    if (!posChanged && !downChanged) return;

    ImGuiIO &io = ImGui::GetIO();
    io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
    if (hasPos) io.AddMousePosEvent(x, y);
    if (downChanged) io.AddMouseButtonEvent(0, down);
}

void Shutdown()
{
    if (g_running.load()) {
        g_running.store(false);
        if (g_thread.joinable()) g_thread.join();
    }
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
    g_ready = false;
}

bool IsReady() { return g_ready; }

void GetDebugText(char *buffer, size_t size)
{
    if (!g_ready) {
        std::snprintf(buffer, size, "触摸：未就绪（见终端输出）");
        return;
    }
    std::lock_guard<std::mutex> lock(g_shared.mutex);
    if (g_shared.touchCount == 0) {
        std::snprintf(buffer, size, "触摸：已就绪（%s），还没读到触摸", g_devicePath);
    } else {
        std::snprintf(buffer, size, "触摸：x=%.0f y=%.0f %s（第 %ld 次按下）",
                      g_shared.x, g_shared.y, g_shared.down ? "按下中" : "已抬起",
                      g_shared.touchCount);
    }
}

} /* namespace TouchInput */
