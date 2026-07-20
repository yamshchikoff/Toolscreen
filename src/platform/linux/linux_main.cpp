#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // For gettid(), RTLD_NEXT
#endif

#include "platform/platform_types.h"

#ifdef PLATFORM_LINUX

#include "platform/linux/x11_display.h"
#include "platform/linux/glx_hook.h"
#include "platform/linux/x11_input.h"
#include "platform/linux/x11_cursor.h"
#include "common/profiler.h"
#include "bootstrap/shared_init.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <atomic>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/syscall.h>

// ---- Global state (mirrors Windows dllmain.cpp globals) ----
// These are the core globals expected by the rest of the codebase.
// In the Windows build they live in dllmain.cpp. For Linux, we provide them here.

// Config (declared extern in utils.h)
#include "gui/gui.h"
Config g_config;
Config g_sharedConfig;
std::atomic<bool> g_configIsDirty{false};
std::atomic<uint64_t> g_configSnapshotVersion{0};

// We'll define additional globals as needed during subsystem integration
std::atomic<bool> g_isShuttingDown{false};

namespace {

std::atomic<bool> g_initialized{false};
std::atomic<bool> g_threadsRunning{false};

// Initialization steps
bool InitPlatform() {
    if (!X11Display::Open()) {
        fprintf(stderr, "[Toolscreen] FATAL: Cannot open X11 display\n");
        return false;
    }
    fprintf(stderr, "[Toolscreen] X11 display opened\n");
    return true;
}

bool InitGLHook() {
    if (!GLXHook::Initialize()) {
        fprintf(stderr, "[Toolscreen] FATAL: Cannot initialize GLX hooks\n");
        return false;
    }
    fprintf(stderr, "[Toolscreen] GLX hooks initialized\n");
    return true;
}

bool InitLogger() {
    // Initialize basic stderr logging
    // Full log file initialization will be done after config is loaded
    fprintf(stderr, "[Toolscreen] Logger initialized (stderr)\n");
    return true;
}

bool LoadConfig() {
    // TODO: Load config from TOML file
    // For now, use embedded defaults
    fprintf(stderr, "[Toolscreen] Config loaded (defaults)\n");
    return true;
}

void StartThreads() {
    // TODO: Start logic thread, file monitor, image monitor
    // These will be connected in later phases
    fprintf(stderr, "[Toolscreen] Background threads started\n");
    g_threadsRunning.store(true);
}

void StopThreads() {
    g_isShuttingDown.store(true);
    g_threadsRunning.store(false);
}

} // namespace

// ---- Module entry/exit points ----

// __attribute__((constructor)) runs when the .so is loaded (before main())
// This is the Linux equivalent of DllMain(DLL_PROCESS_ATTACH)
extern "C" __attribute__((constructor))
void ToolscreenLinuxInit() {
    fprintf(stderr, "[Toolscreen] libtoolscreen.so loaded (constructor)\n");

    // Phase 0: Thread-safety for X11 (must be first X11 call)
    XInitThreads();

    // Phase 0.5: Exception handlers (signals on Linux)
    SharedInit::InstallExceptionHandlers();

    // Phase 1: Platform initialization
    if (!InitPlatform()) {
        fprintf(stderr, "[Toolscreen] Platform init failed, Toolscreen disabled\n");
        return;
    }

    // Phase 2: Logger
    if (!InitLogger()) {
        fprintf(stderr, "[Toolscreen] Logger init failed\n");
        return;
    }

    // Phase 3: GL hooks
    if (!InitGLHook()) {
        fprintf(stderr, "[Toolscreen] GL hook init failed\n");
        return;
    }

    // Phase 4: Config
    SharedInit::InitConfig(g_config, Platform::GetModuleDirectory());
    LoadConfig();

    // Phase 5: Start background threads
    StartThreads();

    g_initialized.store(true);
    fprintf(stderr, "[Toolscreen] Initialization complete\n");
}

// __attribute__((destructor)) runs when the .so is unloaded
// This is the Linux equivalent of DllMain(DLL_PROCESS_DETACH)
extern "C" __attribute__((destructor))
void ToolscreenLinuxShutdown() {
    fprintf(stderr, "[Toolscreen] libtoolscreen.so unloading (destructor)\n");

    StopThreads();
    GLXHook::Shutdown();
    X11Cursor::Shutdown();
    X11Display::Close();

    fprintf(stderr, "[Toolscreen] Shutdown complete\n");
}

// ---- Platform service implementations ----
namespace Platform {

bool Init() { return true; }
void Shutdown() {}

std::string GetModulePath() {
    Dl_info info;
    if (dladdr(reinterpret_cast<const void*>(&GetModulePath), &info) && info.dli_fname) {
        return std::string(info.dli_fname);
    }
    return "";
}

std::string GetModuleDirectory() {
    std::string path = GetModulePath();
    size_t pos = path.rfind('/');
    if (pos != std::string::npos) {
        return path.substr(0, pos);
    }
    return ".";
}

PlatformThreadId GetCurrentThreadId() {
    return static_cast<PlatformThreadId>(syscall(SYS_gettid));
}

void SleepMs(uint32_t ms) {
    usleep(ms * 1000);
}

uint64_t GetTimeUs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL +
           static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
}

uint64_t GetTimeMs() {
    return GetTimeUs() / 1000;
}

std::string GetProcessPath() {
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        return std::string(buf);
    }
    return "";
}

uint32_t GetProcessId() {
    return static_cast<uint32_t>(getpid());
}

void* CreateThread(void (*func)(void*), void* arg) {
    auto* t = new std::thread(func, arg);
    return t;
}

void JoinThread(void* handle) {
    auto* t = static_cast<std::thread*>(handle);
    if (t->joinable()) t->join();
    delete t;
}

int GetMonitorCount() {
    return X11Display::GetMonitorCount();
}

bool GetMonitorRect(int monitorIndex, PlatformRect& outRect) {
    bool isPrimary;
    return X11Display::GetMonitorGeometry(monitorIndex, outRect, isPrimary);
}

bool GetMonitorSize(int monitorIndex, int& outW, int& outH) {
    PlatformRect rect;
    bool isPrimary;
    if (X11Display::GetMonitorGeometry(monitorIndex, rect, isPrimary)) {
        outW = rect.width();
        outH = rect.height();
        return true;
    }
    return false;
}

} // namespace Platform

// ---- Key code conversion implementations ----
PlatformVk PlatformKeyToVk(uint32_t nativeKeycode) {
    // nativeKeycode is an X11 KeySym
    return X11Display::X11KeyToVk(nativeKeycode, 0, 0);
}

uint32_t VkToPlatformKey(PlatformVk vk) {
    return static_cast<uint32_t>(X11Display::VkToX11Keysym(vk));
}

std::string VkToString(PlatformVk vk) {
    KeySym ks = X11Display::VkToX11Keysym(vk);
    if (ks != NoSymbol) {
        std::string s = X11Display::KeysymToString(ks);
        if (!s.empty()) return s;
    }
    // Fallback: return hex code
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%04X", vk);
    return std::string(buf);
}

PlatformVk StringToVk(const std::string& keyStr) {
    // Simple single-key parser
    // For full implementation, integrate with the existing ParseHotkeyString in gui.h
    if (keyStr == "Ctrl" || keyStr == "Control") return Vk::CONTROL;
    if (keyStr == "Shift") return Vk::SHIFT;
    if (keyStr == "Alt") return Vk::MENU;
    if (keyStr == "Win" || keyStr == "Super") return Vk::LWIN;
    if (keyStr == "Space") return Vk::SPACE;
    if (keyStr == "Tab") return Vk::TAB;
    if (keyStr == "Enter" || keyStr == "Return") return Vk::RETURN;
    if (keyStr == "Escape" || keyStr == "Esc") return Vk::ESCAPE;
    if (keyStr == "Backspace") return Vk::BACK;
    if (keyStr == "Delete" || keyStr == "Del") return Vk::DELETE;
    if (keyStr == "Insert" || keyStr == "Ins") return Vk::INSERT;
    if (keyStr == "Home") return Vk::HOME;
    if (keyStr == "End") return Vk::END;
    if (keyStr == "PageUp") return Vk::PRIOR;
    if (keyStr == "PageDown") return Vk::NEXT;
    if (keyStr == "Left") return Vk::LEFT;
    if (keyStr == "Right") return Vk::RIGHT;
    if (keyStr == "Up") return Vk::UP;
    if (keyStr == "Down") return Vk::DOWN;
    if (keyStr == "CapsLock") return Vk::CAPITAL;
    if (keyStr == "NumLock") return Vk::NUMLOCK;
    if (keyStr == "ScrollLock") return Vk::SCROLL;

    // Function keys (strict: F1-F16, no trailing garbage)
    if (keyStr.length() >= 2 && keyStr.length() <= 3 && keyStr[0] == 'F'
        && strspn(keyStr.c_str() + 1, "0123456789") == keyStr.length() - 1) {
        int num = atoi(keyStr.c_str() + 1);
        if (num >= 1 && num <= 16) return Vk::F1 + (num - 1);
    }

    // Single character
    if (keyStr.length() == 1) {
        char c = keyStr[0];
        if (c >= '0' && c <= '9') return Vk::KEY_0 + (c - '0');
        if (c >= 'A' && c <= 'Z') return Vk::KEY_A + (c - 'A');
        if (c >= 'a' && c <= 'z') return Vk::KEY_A + (c - 'a');
    }

    // Try hex format
    if (keyStr.length() >= 2 && keyStr[0] == '0' && (keyStr[1] == 'x' || keyStr[1] == 'X')) {
        return (PlatformVk)strtoul(keyStr.c_str() + 2, nullptr, 16);
    }

    return 0;
}

#endif // PLATFORM_LINUX
