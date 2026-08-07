#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // For gettid(), RTLD_NEXT
#endif

#include "platform/platform_types.h"

#ifdef PLATFORM_LINUX

#include <GL/glew.h>
#include "platform/linux/x11_display.h"
#include "platform/linux/x11_window.h"
#include "platform/linux/glx_hook.h"
#include "platform/linux/x11_input.h"
#include "platform/linux/x11_cursor.h"
#include "common/profiler.h"
#include "bootstrap/shared_init.h"

// X11 #defines None as 0L — must undefine before using GameStateSourceKind::None
#ifdef None
#undef None
#endif
#include "features/game_state_source.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <thread>
#include <atomic>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>

// Log to file (stderr is discarded by Minecraft's JVM).
// Opens/closes on every call — keeps no state, survives signals.
static void TS_LOG(const char* fmt, ...) {
    FILE* f = fopen("/home/user/toolscreen.log", "a");
    if (f) {
        va_list va; va_start(va, fmt); vfprintf(f, fmt, va); va_end(va);
        fclose(f);
    }
}
// Trace log — unbuffered write(), survives crashes better than fopen.
static void TS_TRACE(const char* msg) {
    int fd = open("/home/user/toolscreen_trace.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) { write(fd, msg, strlen(msg)); close(fd); }
}

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
extern const int GRAPHICS_HOOK_CHECK_INTERVAL_MS = 5000;

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

// Additional globals from dllmain.cpp (types from gui.h/utils.h)
GameVersion g_gameVersion;
std::atomic<bool> g_pendingImageLoad{false};
std::mutex g_hotkeyTimestampsMutex;
std::atomic<GLuint> g_cachedGameTextureId{0};

// g_configSnapshot for GetConfigSnapshot/PublishConfigSnapshot
std::atomic<std::shared_ptr<const Config>> g_configSnapshot;

// Remaining globals from dllmain.cpp
std::vector<DecodedImageData> g_decodedImagesQueue;
std::atomic<bool> g_imageDragMode{false};
std::atomic<bool> g_windowOverlayDragMode{false};
std::atomic<bool> g_browserOverlayDragMode{false};
std::atomic<bool> g_mirrorDragMode{false};
std::atomic<bool> g_ninjabrainOverlayDragMode{false};

// More globals (types from game_state_source.h + utils.h)
std::atomic<GameStateSourceKind> g_activeGameStateSource{GameStateSourceKind::None};

// GRAPHICS_HOOK_CHECK_INTERVAL_MS (declared extern const int in utils.h)
// Viewport transition
ViewportTransitionSnapshot g_viewportTransitionSnapshots[2];
std::atomic<int> g_viewportTransitionSnapshotIndex{0};

// Hotkey timestamps (extern unordered_map in gui.h)
std::unordered_map<std::string, std::chrono::steady_clock::time_point> g_hotkeyTimestamps;


// ImGui Win32 stubs (called from gui_runtime.cpp on both platforms)
void ImGui_ImplWin32_Init(void*) {}
void ImGui_ImplWin32_NewFrame() {}
void ImGui_ImplWin32_Shutdown() {}
ImGuiKey ImGui_ImplWin32_KeyEventToImGuiKey(WPARAM, LPARAM) { return ImGuiKey_None; }
int ImGui_ImplWin32_WndProcHandler(void*, unsigned int, uint64_t, int64_t) { return 0; }

// Mode/state (types from gui.h — declared extern, defined in dllmain.cpp)
ModeTransitionAnimation g_modeTransition;
PendingModeSwitch g_pendingModeSwitch;
TempSensitivityOverride g_tempSensitivityOverride{};

// ---- Function implementations from dllmain.cpp ----
std::shared_ptr<const Config> GetConfigSnapshot() {
    return g_configSnapshot.load(std::memory_order_acquire);
}

std::string GetPublishedCurrentModeId() {
    const int index = g_currentModeIdIndex.load(std::memory_order_acquire);
    return g_modeIdBuffers[index];
}

void PublishConfigSnapshot(const Config& config) {
    static std::mutex s_mutex;
    static std::shared_ptr<const Config> s_current;
    std::lock_guard<std::mutex> lock(s_mutex);
    auto newSnap = std::make_shared<const Config>(config);
    s_current = newSnap;
    g_configSnapshot.store(newSnap, std::memory_order_release);
    g_configSnapshotVersion.fetch_add(1, std::memory_order_release);
}

void PublishConfigSnapshot() {
    PublishConfigSnapshot(g_config);
}

bool PublishConfigSnapshotIfUnchanged(const std::shared_ptr<const Config>& expected, const Config& config) {
    if (g_configSnapshot.load(std::memory_order_acquire) == expected) {
        PublishConfigSnapshot(config);
        return true;
    }
    return false;
}

std::string GetHotkeySecondaryMode(size_t index) { return ""; }
void SetHotkeySecondaryMode(size_t index, const std::string& mode) {}
void ResizeHotkeySecondaryModes(size_t newSize) {}

// Timing (declared extern as std::atomic in gui.h — must match)
std::atomic<double> g_lastFrameTimeMs{0.0};
std::atomic<double> g_originalFrameTimeMs{0.0};
std::atomic<int64_t> g_lastGuiToggleTimeMs{0};
std::atomic<int> g_wmMouseMoveCount{0};

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
void BindTextureDirect(GLenum target, GLuint texture) {
    // Linux: real implementation — gloverlay::ScopedState and render code
    // rely on this to restore texture bindings after rendering.
    glBindTexture(target, texture);
}
void InvalidateTrackedGameTextureId(bool, bool) {}
GLuint GetObsCaptureTexture() { return 0; }

// WGL third-party hook stubs (not needed on Linux — LD_PRELOAD handles interposition)
std::atomic<void*> g_wglSwapBuffersThirdPartyHookTarget{nullptr};
std::atomic<void*> g_lastSkippedWglSwapBuffersStart{nullptr};
std::atomic<void*> g_lastSkippedWglSwapBuffersTarget{nullptr};
void* g_owglSwapBuffersThirdParty = nullptr;

// Additional Windows-only stubs
bool ClipCursorDirect(const PlatformRect* r) { return X11Cursor::ClipCursor(r), true; }
BOOL ClipCursorDirect(const RECT* r) { PlatformRect pr{r->left, r->top, r->right, r->bottom}; X11Cursor::ClipCursor(&pr); return TRUE; }
bool ApplyConfineCursorToGameWindow() { return false; }
void ApplyDeferredGuiCursorModeAfterClose() {}
void FinalizeGuiCursorStateAfterClose() {}

// Windows WGL function pointers (not used on Linux — replaced by GLX interposition)
void (*oglViewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
void* owglSwapBuffers = nullptr;

// Windows WGL third-party hook stubs (not used on Linux)
int hkwglSwapBuffers(void*) { return 0; }
int hkwglSwapBuffers_ThirdParty(void*) { return 0; }

// Viewport/mode stubs (defined in dllmain.cpp on Windows)
#include "common/utils.h"  // ModeViewportInfo
bool ResolvePresentedGameViewport(ModeViewportInfo& outViewport) { outViewport.valid = false; return false; }
bool GetLatestGameViewportSize(int& w, int& h) { w = 1920; h = 1080; return true; }
void InvalidateLatestGameViewportSize() {}

namespace {

std::atomic<bool> g_initialized{false};
std::atomic<bool> g_threadsRunning{false};

// Initialization steps
bool InitPlatform() {
    if (!X11Display::Open()) {
        TS_LOG("[Toolscreen] FATAL: Cannot open X11 display\n");
        return false;
    }
    TS_LOG("[Toolscreen] X11 display opened\n");
    return true;
}

bool InitGLHook() {
    if (!GLXHook::Initialize()) {
        TS_LOG("[Toolscreen] FATAL: Cannot initialize GLX hooks\n");
        return false;
    }
    TS_LOG("[Toolscreen] GLX hooks initialized\n");
    return true;
}

bool InitLogger() {
    // Initialize basic stderr logging
    // Full log file initialization will be done after config is loaded
    TS_LOG("[Toolscreen] Logger initialized (stderr)\n");
    return true;
}

bool LoadToolscreenConfig() {
    // TODO: Load config from TOML file
    // For now, use embedded defaults
    TS_LOG("[Toolscreen] Config loaded (defaults)\n");
    return true;
}

void StartThreads() {
    // TODO: Start logic thread, file monitor, image monitor
    // These will be connected in later phases
    TS_LOG("[Toolscreen] Background threads started\n");
    g_threadsRunning.store(true);
}

void StopThreads() {
    g_isShuttingDown.store(true);
    g_threadsRunning.store(false);
}

} // namespace

// stb_image implementation (defined in dllmain.cpp on Windows)
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

// ---- Module entry/exit points ----

// Lazy initialization flag — defer all X11/GL work until first glXSwapBuffers call.
// This avoids conflicts with Java classloaders (Fabric, Forge) that trigger
// during dlopen when the JVM is not yet ready for X11 connections.
static std::once_flag g_lazyInitFlag;

void ToolscreenLazyInit() {
    std::call_once(g_lazyInitFlag, []() {
        TS_TRACE("[Toolscreen] LazyInit: enter\n");
        TS_LOG("[Toolscreen] Lazy init triggered\n");

        XInitThreads();
        TS_TRACE("[Toolscreen] LazyInit: XInitThreads done\n");
        // NOTE: NO signal handlers — JVM uses SIGSEGV internally

        if (!InitPlatform()) {
            TS_LOG("[Toolscreen] Platform init failed, Toolscreen disabled\n");
            TS_TRACE("[Toolscreen] LazyInit: InitPlatform FAILED\n");
            return;
        }
        TS_TRACE("[Toolscreen] LazyInit: InitPlatform OK\n");

        // Открываем второе Display-соединение для ресайза в обход GLFW
        X11Window::InitOwnDisplay();

        if (!InitLogger()) {
            TS_LOG("[Toolscreen] Logger init failed\n");
            TS_TRACE("[Toolscreen] LazyInit: InitLogger FAILED\n");
            return;
        }
        TS_TRACE("[Toolscreen] LazyInit: InitLogger OK\n");

        if (!InitGLHook()) {
            TS_LOG("[Toolscreen] GL hook init failed\n");
            TS_TRACE("[Toolscreen] LazyInit: InitGLHook FAILED\n");
            return;
        }
        TS_TRACE("[Toolscreen] LazyInit: InitGLHook OK\n");

        SharedInit::InitConfig(g_config, Platform::GetModuleDirectory());
        TS_TRACE("[Toolscreen] LazyInit: InitConfig done\n");
        LoadToolscreenConfig();
        StartThreads();
        g_initialized.store(true);
        TS_LOG("[Toolscreen] Initialization complete\n");
        TS_TRACE("[Toolscreen] LazyInit: complete\n");
    });
}

static void DBG_PRINT(const char* msg) {
    int fd = open("/tmp/toolscreen_dbg.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) _exit(99);
    write(fd, msg, strlen(msg));
    fsync(fd);
    close(fd);
}

extern "C" __attribute__((constructor))
void ToolscreenLinuxInit() {
    DBG_PRINT("CTOR: enter\n");
    TS_TRACE("[Toolscreen] constructor: enter\n");
    DBG_PRINT("CTOR: after TS_TRACE\n");
    TS_LOG("[Toolscreen] libtoolscreen.so loaded (constructor)\n");
    DBG_PRINT("CTOR: after TS_LOG\n");
    TS_TRACE("[Toolscreen] constructor: calling InstallRuntimeHook\n");
    DBG_PRINT("CTOR: before InstallRuntimeHook\n");
    GLXHook::InstallRuntimeHook();
    DBG_PRINT("CTOR: after InstallRuntimeHook\n");
    TS_TRACE("[Toolscreen] constructor: done\n");
    DBG_PRINT("CTOR: done\n");
}

// __attribute__((destructor)) runs when the .so is unloaded
// This is the Linux equivalent of DllMain(DLL_PROCESS_DETACH)
extern "C" __attribute__((destructor))
void ToolscreenLinuxShutdown() {
    TS_LOG("[Toolscreen] libtoolscreen.so unloading (destructor)\n");

    StopThreads();
    X11Window::CloseOwnDisplay();
    GLXHook::Shutdown();
    X11Cursor::Shutdown();
    X11Display::Close();

    TS_LOG("[Toolscreen] Shutdown complete\n");
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
