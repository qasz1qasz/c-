/*
 * volume_keys.cpp - 见 volume_keys.h 的说明。
 */

#include "volume_keys.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

namespace VolumeKeys {

static std::atomic<bool> g_running{false};
static std::atomic<bool> g_showRequest{false};
static std::atomic<bool> g_hideRequest{false};
static std::thread g_thread;
static std::vector<int> g_fds;

static bool HasKey(int fd, int code)
{
    /* EVIOCGBIT(EV_KEY, ...) 返回按键位图，code 对应的位为 1 表示设备有这个键 */
    unsigned long bits[(KEY_MAX + 8 * (int)sizeof(long)) / (8 * (int)sizeof(long))];
    std::memset(bits, 0, sizeof(bits));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, (int)sizeof(bits)), bits) < 0) return false;
    int idx = code / (8 * (int)sizeof(long));
    int shift = code % (8 * (int)sizeof(long));
    return ((bits[idx] >> shift) & 1UL) != 0;
}

static void ReaderThread()
{
    std::vector<struct pollfd> pfds;
    for (int fd : g_fds) {
        struct pollfd p{};
        p.fd = fd;
        p.events = POLLIN;
        pfds.push_back(p);
    }

    while (g_running.load()) {
        int ret = poll(pfds.data(), pfds.size(), 100);
        if (ret <= 0) continue;

        for (auto &p : pfds) {
            if (!(p.revents & POLLIN)) continue;
            struct input_event events[32];
            ssize_t n = read(p.fd, events, sizeof(events));
            if (n <= 0) continue;
            int count = (int)(n / (ssize_t)sizeof(struct input_event));
            for (int i = 0; i < count; i++) {
                const struct input_event &e = events[i];
                if (e.type != EV_KEY || e.value != 1) continue; /* 只认按下瞬间 */
                if (e.code == KEY_VOLUMEUP) g_showRequest.store(true);
                else if (e.code == KEY_VOLUMEDOWN) g_hideRequest.store(true);
            }
        }
    }
}

bool Init()
{
    Shutdown();

    char path[64];
    for (int i = 0; i < 32; i++) {
        std::snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;
        if (HasKey(fd, KEY_VOLUMEUP) || HasKey(fd, KEY_VOLUMEDOWN)) {
            printf("音量键：监听设备 %s\n", path);
            g_fds.push_back(fd);
        } else {
            close(fd);
        }
    }
    if (g_fds.empty()) {
        printf("音量键：没找到带音量键的输入设备（或无权打开），音量键开关不可用。\n");
        return false;
    }

    g_running.store(true);
    g_thread = std::thread(ReaderThread);
    return true;
}

void Shutdown()
{
    if (g_running.load()) {
        g_running.store(false);
        if (g_thread.joinable()) g_thread.join();
    }
    for (int fd : g_fds) close(fd);
    g_fds.clear();
}

bool ConsumeShowRequest() { return g_showRequest.exchange(false); }
bool ConsumeHideRequest() { return g_hideRequest.exchange(false); }

} /* namespace VolumeKeys */
