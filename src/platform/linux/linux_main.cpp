#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // For gettid(), RTLD_NEXT
#endif

#include "platform/platform_types.h"

#ifdef PLATFORM_LINUX

#include <GL/glew.h>
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
// WARNING: many are stubs (false/0) — full functionality needs the render pipeline.

// Config (declared extern in utils.h)
#include "gui/gui.h"
Config g_config;
Config g_sharedConfig;
std::atomic<bool> g_configIsDirty{false};
std::atomic<uint64_t> g_configSnapshotVersion{0};

// Shutdown
std::atomic<bool> g_isShuttingDown{false};

// Game window
std::atomic<HWND> g_minecraftHwnd{nullptr};
std::atomic<bool> g_gameWindowActive{false};
std::atomic<HWND> g_subclassedHwnd{nullptr};
WNDPROC g_originalWndProc = nullptr;

// Config/state
std::atomic<bool> g_configLoaded{false};
std::atomic<bool> g_configLoadFailed{false};
std::string g_configLoadError;
std::mutex g_configErrorMutex;
std::wstring g_toolscreenPath;
std::wstring g_modeFilePath;
std::wstring g_stateFilePath;
std::wstring g_hermesAliveFilePath;
std::wstring g_stateOutputFilePath;
std::atomic<bool> g_isStateOutputAvailable{false};

// GUI visibility
std::atomic<bool> g_showGui{false};
std::atomic<bool> g_imageOverlaysVisible{true};
std::atomic<bool> g_windowOverlaysVisible{true};
std::atomic<bool> g_ninjabrainOverlayVisible{true};
std::atomic<bool> g_browserOverlaysVisible{true};
std::atomic<bool> g_guiNeedsRecenter{false};
std::atomic<bool> g_overlayEditorMode{false};

// Mode
std::string g_currentModeId;
std::mutex g_modeIdMutex;
std::string g_modeIdBuffers[2];
std::atomic<int> g_currentModeIdIndex{0};
std::atomic<bool> g_screenshotRequested{false};

// Cursor
std::atomic<bool> g_cursorsNeedReload{false};
std::atomic<bool> g_glfwCursorGrabbed{false};
std::atomic<bool> g_wasCursorVisible{true};
std::atomic<bool> g_forceVisibleCursorWhileGuiOpen{false};
std::atomic<HCURSOR> g_specialCursorHandle{nullptr};

// Graphics hook
std::atomic<bool> g_graphicsHookDetected{false};
std::atomic<HMODULE> g_graphicsHookModule{nullptr};
std::chrono::steady_clock::time_point g_lastGraphicsHookCheck;
const int GRAPHICS_HOOK_CHECK_INTERVAL_MS = 5000;

// Hotkeys (declared extern in utils.h / gui.h — defined in gui.cpp on Windows)
std::mutex g_hotkeyMainKeysMutex;
std::vector<DWORD> g_hotkeyMainKeys;
std::mutex g_triggerOnReleaseMutex;
std::string g_triggerOnReleasePending;
std::string g_triggerOnReleaseInvalidated;

// Mirror selection
std::string g_currentlyEditingMirror;
std::string g_selectedMirrorName;
int g_selectedMirrorOutW = 0, g_selectedMirrorOutH = 0;
int g_selectedMirrorScreenX = 0, g_selectedMirrorScreenY = 0;
int g_selectedMirrorScreenW = 0, g_selectedMirrorScreenH = 0;
std::string g_scrollToMirrorName;

// Window overlay selection
std::string g_selectedWindowOverlayName;
int g_selectedWindowOverlayScreenX = 0, g_selectedWindowOverlayScreenY = 0;
int g_selectedWindowOverlayScreenW = 0, g_selectedWindowOverlayScreenH = 0;
std::string g_scrollToWindowOverlayName;
bool g_windowOverlayCropMode = false;

// Image selection
std::string g_selectedImageName;
int g_selectedImageScreenX = 0, g_selectedImageScreenY = 0;
int g_selectedImageScreenW = 0, g_selectedImageScreenH = 0;
std::string g_scrollToImageName;
bool g_imageCropMode = false;

// Drag modes (declared extern in utils.h)

// Interactive create
std::atomic<bool> g_interactiveCreateCancel{false};
std::atomic<bool> g_interactiveCreateRequested{false};
std::atomic<bool> g_interactiveCreateRelativeToScreen{false};
std::atomic<int> g_interactiveCreateStage{0};

// Pending mode switch (types from render.h — declared extern in utils.h)
std::mutex g_pendingModeSwitchMutex;
std::mutex g_modeTransitionMutex;

// Temp sensitivity
std::mutex g_tempSensitivityMutex;

// Game state
std::string g_gameStateBuffers[2];
std::atomic<int> g_currentGameStateIndex{0};

// Images
std::atomic<bool> g_allImagesLoaded{false};
std::mutex g_decodedImagesMutex;

// Logging
std::ofstream logFile;
std::mutex g_logFileMutex;

// Monitoring
std::atomic<bool> g_stopMonitoring{false};
std::atomic<bool> g_stopImageMonitoring{false};

// Misc
HMODULE g_hModule = nullptr;

// ---- Function stubs for symbols expected by the codebase ----
// These are defined in Windows .cpp files; on Linux they are stubs.

void ApplyWindowsMouseSpeed() {}
void RestoreWindowsMouseSpeed() {}
void ApplyKeyRepeatSettings() {}
void RestoreKeyRepeatSettings() {}
void ClearTempSensitivityOverride() {}
void GetEffectiveKeyRepeatTimings(int& delay, int& rate) { delay = 0; rate = 0; }
void RebuildHotkeyMainKeys_Internal() {}
void ResetAllHotkeySecondaryModes() {}
void ResetAllHotkeySecondaryModes(const Config&) {}

bool BlitFramebufferDirect(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum) { return false; }
void BindTextureDirect(GLenum, GLuint) {}
void InvalidateTrackedGameTextureId(bool, bool) {}
GLuint GetObsCaptureTexture() { return 0; }

bool ClipCursorDirect(const PlatformRect* r) { return X11Cursor::ClipCursor(r), true; }
bool ApplyConfineCursorToGameWindow() { return false; }
void ApplyDeferredGuiCursorModeAfterClose() {}
void FinalizeGuiCursorStateAfterClose() {}

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

bool LoadToolscreenConfig() {
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
    LoadToolscreenConfig();

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

// StringToVk is defined in gui_controls.cpp (shared implementation)
// PlatformVk StringToVk = DWORD StringToVk (same type on Linux)

#endif // PLATFORM_LINUX
