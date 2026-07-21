#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // For RTLD_NEXT
#endif

#ifdef PLATFORM_LINUX
// GLEW must be first GL header — before glx_hook.h (which pulls <GL/glx.h> → <GL/gl.h>)
#include <GL/glew.h>
#endif

#include "glx_hook.h"
#include "x11_display.h"
#include "platform/linux/x11_input.h"
#include "gui/imgui_impl_x11.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "common/profiler.h"
// X11 #defines None as 0L — must undefine before gl_overlay.h (which
// transitively includes game_state_source.h with enum class None = 0)
#ifdef None
#undef None
#endif
#include "common/gl_overlay.h"

#ifdef PLATFORM_LINUX
// Lazy init from linux_main.cpp (avoids X11 in constructor)
extern void ToolscreenLazyInit();
#include <GL/glx.h>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace GLXHook {

// ---- Original function pointers ----
// glXSwapBuffers: resolved via dlsym(RTLD_NEXT) inside the interposed function itself.
// Other GL functions: also resolved via dlsym(RTLD_NEXT) in their respective
// LD_PRELOAD interpositions below. We keep raw pointers (not atomic) because
// std::call_once guarantees they are fully written before any reader can observe them.
std::atomic<SwapBuffersFunc> g_realSwapBuffers{nullptr};
ViewportFunc g_realViewport = nullptr;
BindTextureFunc g_realBindTexture = nullptr;
BindFramebufferFunc g_realBindFramebuffer = nullptr;
BlitFramebufferFunc g_realBlitFramebuffer = nullptr;

// ---- Symbol interposition via LD_PRELOAD ----
// On Linux, we intercept GL / GLX functions by exporting our own copies from the .so.
// The dynamic linker resolves to our copy first when LD_PRELOAD is used.
// Each interposed function resolves the real implementation via dlsym(RTLD_NEXT, ...)
// on its FIRST call (thread-safe via std::call_once), then chains to it.
//
// This pattern is used for ALL hooked GL functions — not just glXSwapBuffers.
// It avoids the torn-read race that an inline-hook engine (mprotect + memcpy)
// would cause when patching code that other threads are concurrently executing.

namespace {

// ---- Inline hook engine (safe for multithreaded use) ----
// x86-64 only: installs a 5-byte relative jump: jmp rel32 (E9 + int32 offset).
//
// Unlike the old 14-byte absolute jump (mov rax, imm64; jmp rax), a 5-byte
// jmp rel32 fits within a single 8-byte aligned write — which is atomic on
// x86-64. Other threads see either the original code or the complete jump,
// never a torn instruction. The page is kept RWX during the write (X remains
// set), so other threads don't fault while we modify it.
//
// Limitation: ±2 GB range. The detour must be within 2 GB of the target.
// For same-process .so injection this is always satisfied.

constexpr size_t kJumpSize     = 5;   // jmp rel32 (atomic at target)
constexpr size_t kBackupSize   = 16;  // bytes to backup (must cover ≥ kJumpSize
                                      // and align to instruction boundary)
constexpr size_t kAtomSize     = 8;   // atomic write alignment
constexpr size_t kBridgeSize   = 64;  // small page for bridge trampoline
constexpr size_t kTrampSize    = 128; // trampoline allocation

struct HookEntry {
    void* target;
    void* detour;
    void* bridge;              // Near-target jump bridge (for >2GB)
    void* trampoline;          // Callable original: backup bytes + jmp target+5
    uint8_t backup[kBackupSize]; // Original bytes at target
    size_t bridgeSize;         // Size of bridge allocation
    size_t trampolineSize;     // Size of trampoline allocation
    bool enabled;
};

std::mutex g_hookMutex;
std::unordered_map<void*, HookEntry> g_hooks;
HookEntry* g_activeHook = nullptr;  // The glXSwapBuffers hook entry
std::atomic<bool> g_initialized{false};
std::atomic<bool> g_hooksEnabled{false};
std::atomic<bool> g_glewReady{false};

// Trace helper (HOOK_LOG is not visible inside anonymous namespace)
static void DBG_TRACE(const char* msg) {
    int fd = open("/home/user/toolscreen_trace.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) { write(fd, msg, strlen(msg)); close(fd); }
}

void* PageAlign(void* addr) {
    long pageSize = sysconf(_SC_PAGESIZE);
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(addr) & ~(pageSize - 1));
}

size_t PageSpan(void* addr, size_t size) {
    long pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t start = reinterpret_cast<uintptr_t>(PageAlign(addr));
    uintptr_t end = reinterpret_cast<uintptr_t>(addr) + size;
    end = (end + pageSize - 1) & ~(pageSize - 1);
    return end - start;
}

// Allocate RWX memory within ±2GB of `near_addr`. Tries downward from near_addr
// then upward. Returns nullptr if no free page within range.
static void* AllocateNear(void* near_addr, size_t size) {
    uintptr_t target_page = reinterpret_cast<uintptr_t>(PageAlign(near_addr));
    uintptr_t low  = target_page > 0x80000000UL ? target_page - 0x7FFF0000UL : 0x10000UL;
    uintptr_t high = target_page + 0x7FFF0000UL;

    // Scan downward
    for (uintptr_t addr = target_page - 0x1000; addr >= low; addr -= 0x1000) {
        void* p = mmap(reinterpret_cast<void*>(addr), size,
                       PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (p != MAP_FAILED) return p;
    }
    // Scan upward
    for (uintptr_t addr = target_page + 0x1000; addr < high; addr += 0x1000) {
        void* p = mmap(reinterpret_cast<void*>(addr), size,
                       PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (p != MAP_FAILED) return p;
    }
    return nullptr;
}

// Install a 5-byte atomic jmp rel32 at target → destination.
// Requires |dest - target| < 2GB. Returns true on success.
static bool WriteRelJump(void* target, void* destination, uint8_t* backup) {
    // Backup enough bytes for the trampoline to execute a full prologue
    memcpy(backup, target, kBackupSize);

    size_t span = PageSpan(target, kJumpSize);
    if (mprotect(PageAlign(target), span, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        DBG_TRACE("[Toolscreen] WriteRelJump: mprotect failed\n");
        return false;
    }

    uint8_t newBytes[kAtomSize];
    memcpy(newBytes, target, kAtomSize);
    newBytes[0] = 0xE9;  // JMP rel32
    int64_t rel = static_cast<uint8_t*>(destination) - (static_cast<uint8_t*>(target) + 5);
    if (rel < INT32_MIN || rel > INT32_MAX) {
        mprotect(PageAlign(target), span, PROT_READ | PROT_EXEC);
        return false;
    }
    int32_t rel32 = static_cast<int32_t>(rel);
    memcpy(&newBytes[1], &rel32, 4);

    uint64_t newVal;
    memcpy(&newVal, newBytes, 8);
    __atomic_store_n(reinterpret_cast<uint64_t*>(target), newVal, __ATOMIC_SEQ_CST);

    mprotect(PageAlign(target), span, PROT_READ | PROT_EXEC);
    return true;
}

// Write a 14-byte absolute jump: mov rax, imm64; jmp rax
// No distance limit. Used only in the bridge trampoline (single-threaded context).
static void WriteAbsJump(void* addr, void* destination) {
    uint8_t* code = static_cast<uint8_t*>(addr);
    code[0]  = 0x48; // REX.W
    code[1]  = 0xB8; // MOV RAX, imm64
    memcpy(&code[2], &destination, 8);
    code[10] = 0xFF; // JMP RAX
    code[11] = 0xE0;
}

// Main hook install: atomically writes 5-byte jmp rel32 at target, pointing to
// a bridge page allocated within 2GB of target. The bridge contains a 14-byte
// absolute jump to the final destination (no distance limit).
bool InstallJump(void* target, void* destination, uint8_t* backup) {
    char buf[256];
    int64_t dist = static_cast<uint8_t*>(destination) - static_cast<uint8_t*>(target);
    snprintf(buf, sizeof(buf),
        "[Toolscreen] InstallJump: target=%p dest=%p dist=%ldM\n",
        target, destination, (long)(dist / (1024*1024)));
    DBG_TRACE(buf);

    // Try direct 5-byte jump first (faster, no bridge needed)
    if (dist > (int64_t)(INT32_MIN) && dist < (int64_t)(INT32_MAX)) {
        if (WriteRelJump(target, destination, backup)) {
            DBG_TRACE("[Toolscreen] InstallJump: direct jmp rel32 OK\n");
            return true;
        }
    }

    // Distance >2GB — need a bridge trampoline near target
    DBG_TRACE("[Toolscreen] InstallJump: distance >2GB, allocating bridge\n");
    void* bridge = AllocateNear(target, kBridgeSize);
    if (!bridge) {
        DBG_TRACE("[Toolscreen] InstallJump: bridge allocation FAILED\n");
        return false;
    }
    snprintf(buf, sizeof(buf),
        "[Toolscreen] InstallJump: bridge=%p dist_to_target=%ldM\n",
        bridge, (long)((static_cast<uint8_t*>(bridge) - static_cast<uint8_t*>(target)) / (1024*1024)));
    DBG_TRACE(buf);

    // Write 14-byte absolute jump in bridge → destination
    WriteAbsJump(bridge, destination);

    // Write 5-byte atomic jmp rel32 at target → bridge
    if (!WriteRelJump(target, bridge, backup)) {
        DBG_TRACE("[Toolscreen] InstallJump: rel jump to bridge FAILED\n");
        munmap(bridge, kBridgeSize);
        return false;
    }

    DBG_TRACE("[Toolscreen] InstallJump: bridge jump OK\n");
    return true;
}

// Write the original 8 bytes back to `target`.
void RestoreJump(void* target, const uint8_t* backup) {
    size_t span = PageSpan(target, kJumpSize);
    if (mprotect(PageAlign(target), span, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) return;
    uint64_t oldVal;
    memcpy(&oldVal, backup, 8);
    __atomic_store_n(reinterpret_cast<uint64_t*>(target), oldVal, __ATOMIC_SEQ_CST);
    mprotect(PageAlign(target), span, PROT_READ | PROT_EXEC);
}

// Routes X11 input events to ImGui. Called from X11Input::PollEvents() via
// the event callback set up when the game window is first detected.
std::atomic<bool> g_inputWired{false};

bool RouteX11EventToImGui(const X11Input::InputEvent& ev) {
    using ET = X11Input::EventType;
    switch (ev.type) {
    case ET::KeyDown:
        return ImGui_ImplX11_HandleKeyEvent(ev.scanCode, true, 0);
    case ET::KeyUp:
        return ImGui_ImplX11_HandleKeyEvent(ev.scanCode, false, 0);
    case ET::MouseDown: {
        // Map Vk mouse codes to ImGui button indices (0=left, 1=right, 2=middle)
        int btn = -1;
        if (ev.vkCode == Vk::MOUSE_LEFT)   btn = 0;
        if (ev.vkCode == Vk::MOUSE_RIGHT)  btn = 1;
        if (ev.vkCode == Vk::MOUSE_MIDDLE) btn = 2;
        if (btn >= 0) return ImGui_ImplX11_HandleMouseButtonEvent(btn, true);
        return false;
    }
    case ET::MouseUp: {
        int btn = -1;
        if (ev.vkCode == Vk::MOUSE_LEFT)   btn = 0;
        if (ev.vkCode == Vk::MOUSE_RIGHT)  btn = 1;
        if (ev.vkCode == Vk::MOUSE_MIDDLE) btn = 2;
        if (btn >= 0) return ImGui_ImplX11_HandleMouseButtonEvent(btn, false);
        return false;
    }
    case ET::MouseMove:
        return ImGui_ImplX11_HandleMouseMotionEvent(ev.mouseX, ev.mouseY);
    case ET::MouseWheel:
        return ImGui_ImplX11_HandleMouseWheelEvent(ev.mouseDelta);
    default:
        return false;
    }
}

} // namespace

// Utility log functions (available to all functions in GLXHook namespace)
static void HOOK_LOG(const char* fmt, ...) {
    FILE* f = fopen("/home/user/toolscreen.log", "a");
    if (f) { va_list va; va_start(va, fmt); vfprintf(f, fmt, va); va_end(va); fclose(f); }
}
static void TRACE_CALL(const char* msg) {
    int fd = open("/home/user/toolscreen_trace.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) { write(fd, msg, strlen(msg)); close(fd); }
}

bool CreateHook(void* target, void* detour, void** original) {
    std::lock_guard<std::mutex> lock(g_hookMutex);

    if (!target || !detour) return false;

    auto it = g_hooks.find(target);
    if (it != g_hooks.end()) {
        if (original) *original = it->second.trampoline;
        return true;
    }

    HookEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.target = target;
    entry.detour = detour;
    entry.enabled = false;

    // Install the jump from target to detour (via bridge if >2GB)
    if (!InstallJump(target, detour, entry.backup)) {
        DBG_TRACE("[Toolscreen] CreateHook: InstallJump failed\n");
        return false;
    }

    // Create callable trampoline: [backup kBackupSize bytes] [jmp to target+kBackupSize]
    // kBackupSize (16) covers the full function prologue (endbr64 + mov + ...)
    // so the jump lands on a proper instruction boundary, not mid-instruction.
    size_t trampSize = kTrampSize;
    void* tramp = mmap(nullptr, trampSize, PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (tramp != MAP_FAILED) {
        memcpy(tramp, entry.backup, kBackupSize);
        uint8_t* jmp = static_cast<uint8_t*>(tramp) + kBackupSize;
        jmp[0] = 0xE9;
        int64_t rel = (static_cast<uint8_t*>(target) + kBackupSize) - (jmp + 5);
        int32_t rel32 = static_cast<int32_t>(rel);
        memcpy(&jmp[1], &rel32, 4);
        mprotect(tramp, trampSize, PROT_READ | PROT_EXEC);
        entry.trampoline = tramp;
        entry.trampolineSize = trampSize;
    }

    entry.enabled = true;

    if (original) *original = entry.trampoline;
    g_hooks[target] = entry;
    return true;
}

bool EnableHook(void* hook) {
    std::lock_guard<std::mutex> lock(g_hookMutex);
    auto it = g_hooks.find(hook);
    if (it == g_hooks.end()) return false;
    if (it->second.enabled) return true; // Already enabled
    InstallJump(it->second.target, it->second.detour, it->second.backup);
    it->second.enabled = true;
    return true;
}

bool DisableHook(void* hook) {
    std::lock_guard<std::mutex> lock(g_hookMutex);
    auto it = g_hooks.find(hook);
    if (it == g_hooks.end()) return false;
    if (!it->second.enabled) return true; // Already disabled
    RestoreJump(it->second.target, it->second.backup);
    it->second.enabled = false;
    return true;
}

bool EnableAllHooks() {
    g_hooksEnabled.store(true);
    return true;
}

void* GetGLFunc(const char* name) {
    // Try glXGetProcAddress first (for GLX and modern GL functions)
    void* func = reinterpret_cast<void*>(glXGetProcAddress(reinterpret_cast<const GLubyte*>(name)));
    if (func) return func;

    // Fall back to dlsym (for older GL functions in libGL)
    func = dlsym(RTLD_DEFAULT, name);
    return func;
}

// ---- glXSwapBuffers interposition ----
// Uses a thread_local guard against re-entrancy (our own rendering may
// trigger another swap). Resolution via std::call_once for thread safety.

// GLEW #defines glBindFramebuffer/glBlitFramebuffer as macros pointing to
// __glew* stubs. Undefine them so our LD_PRELOAD interpositions keep their
// real names and are properly exported to the dynamic symbol table.
#undef glBindFramebuffer
#undef glBlitFramebuffer

// LD_PRELOAD interposed functions must be exported despite -fvisibility=hidden
#pragma GCC visibility push(default)
extern "C" {

void glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
    // Re-entrancy guard
    static thread_local int swapDepth = 0;
    if (swapDepth > 0) {
        if (g_realSwapBuffers.load()) {
            g_realSwapBuffers.load()(dpy, drawable);
        }
        return;
    }
    ++swapDepth;

    // Resolve the real glXSwapBuffers on first call (thread-safe)
    static std::once_flag swapOnceFlag;
    std::call_once(swapOnceFlag, []() {
        g_realSwapBuffers.store(reinterpret_cast<SwapBuffersFunc>(
            dlsym(RTLD_NEXT, "glXSwapBuffers")));
        if (!g_realSwapBuffers.load()) {
            HOOK_LOG("[Toolscreen] FATAL: Cannot find real glXSwapBuffers\n");
        }
    });

    if (!g_realSwapBuffers.load()) {
        --swapDepth;
        return;
    }

    // Render overlays BEFORE the real swap so they appear on the current frame
    hk_glXSwapBuffers(dpy, drawable);

    // Call the real glXSwapBuffers (game frame is presented with our overlays)
    g_realSwapBuffers.load()(dpy, drawable);

    --swapDepth;
}

// ---- Interposed GL functions (LD_PRELOAD, no inline hooks) ----
// Each overrides the system GL function. The real implementation is resolved
// via dlsym(RTLD_NEXT) on first call (std::call_once). This avoids the
// torn-read race that inline-hooking would cause during concurrent rendering.

__attribute__((used))
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        g_realViewport = reinterpret_cast<ViewportFunc>(dlsym(RTLD_NEXT, "glViewport"));
    });
    hk_glViewport(x, y, width, height);
}

__attribute__((used))
void glBindTexture(GLenum target, GLuint texture) {
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        g_realBindTexture = reinterpret_cast<BindTextureFunc>(dlsym(RTLD_NEXT, "glBindTexture"));
    });
    hk_glBindTexture(target, texture);
}

__attribute__((used))
void glBindFramebuffer(GLenum target, GLuint framebuffer) {
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        g_realBindFramebuffer = reinterpret_cast<BindFramebufferFunc>(dlsym(RTLD_NEXT, "glBindFramebuffer"));
    });
    hk_glBindFramebuffer(target, framebuffer);
}

__attribute__((used))
void glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                       GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                       GLbitfield mask, GLenum filter) {
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        g_realBlitFramebuffer = reinterpret_cast<BlitFramebufferFunc>(dlsym(RTLD_NEXT, "glBlitFramebuffer"));
    });
    hk_glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

} // extern "C"
#pragma GCC visibility pop

// ---- Hooked function implementations (called from interposed stubs) ----

void hk_glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
    PROFILE_SCOPE("glXSwapBuffers");
    TRACE_CALL("[Toolscreen] hk_glXSwapBuffers: ENTER\n");

    // Recursion guard: if we called realSwap, it calls the original
    // glXSwapBuffers which should NOT re-enter our hook.
    static thread_local bool inHkSwap = false;
    if (inHkSwap) {
        // Re-entered — just return, glXSwapBuffersMscOML handles the swap
        return;
    }
    inHkSwap = true;

    // Step 1: Lazy init (X11, config — NO GLEW, NO ImGui)
    ToolscreenLazyInit();

    // Step 2: Call the real implementation via saved dispatch pointer.
    // g_realSwapBuffers was saved from the dispatch table before patching.
    inHkSwap = false;
    auto* realImpl = g_realSwapBuffers.load(std::memory_order_acquire);
    if (realImpl) {
        realImpl(dpy, drawable);
    }
    inHkSwap = false;

    // Lazy GLEW initialization on first call (requires an active GL context)
    static std::atomic<GLXContext> s_glewContext{nullptr};
    GLXContext currentCtx = glXGetCurrentContext();
    GLXContext prevCtx = s_glewContext.load(std::memory_order_acquire);
    bool contextChanged = (currentCtx != prevCtx);
    if (contextChanged) {
        HOOK_LOG("[Toolscreen] GL context changed: %p → %p, resetting GLEW\n",
                reinterpret_cast<void*>(prevCtx),
                reinterpret_cast<void*>(currentCtx));
        g_glewReady.store(false, std::memory_order_release);
        s_glewContext.store(currentCtx, std::memory_order_release);
    }
    if (!g_glewReady.load(std::memory_order_acquire)) {
        TRACE_CALL("[Toolscreen] glewInit start\n");
        GLenum glewErr = glewInit();
        TRACE_CALL("[Toolscreen] glewInit done\n");
        if (glewErr == GLEW_OK) {
            g_glewReady.store(true, std::memory_order_release);
            s_glewContext.store(glXGetCurrentContext(), std::memory_order_release);
            HOOK_LOG("[Toolscreen] GLEW initialized OK (context %p)\n",
                    reinterpret_cast<void*>(s_glewContext.load()));
        } else {
            HOOK_LOG("[Toolscreen] GLEW init failed: %s\n",
                    reinterpret_cast<const char*>(glewGetErrorString(glewErr)));
        }
    }

    // Create ImGui context (once, thread-safe)
    static std::once_flag g_imguiInitFlag;
    static std::atomic<bool> g_imguiInitialized{false};
    static ImGuiContext* g_imguiCtx = nullptr;
    if (!g_imguiInitialized.load(std::memory_order_acquire) && g_glewReady.load()) {
        TRACE_CALL("[Toolscreen] ImGui init: entering call_once\n");
        std::call_once(g_imguiInitFlag, [&]() {
            TRACE_CALL("[Toolscreen] ImGui init: ScopedState\n");
            gloverlay::ScopedState initState;
            TRACE_CALL("[Toolscreen] ImGui init: CreateContext\n");
            IMGUI_CHECKVERSION();
            g_imguiCtx = ImGui::CreateContext();
            ImGui::SetCurrentContext(g_imguiCtx);
            TRACE_CALL("[Toolscreen] ImGui init: ImplOpenGL3_Init\n");
            ImGui_ImplOpenGL3_Init("#version 330");
            TRACE_CALL("[Toolscreen] ImGui init: config\n");
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            ImGui::GetStyle().FrameRounding = 3.0f;
            g_imguiInitialized.store(true, std::memory_order_release);
            HOOK_LOG("[Toolscreen] ImGui initialized (X11/OpenGL3)\n");
            TRACE_CALL("[Toolscreen] ImGui init: done\n");
        });
    }

    // Detect/update game window from current GLX drawable
    {
        GLXDrawable currentDrawable = glXGetCurrentDrawable();
        if (currentDrawable) {
            Window win = static_cast<Window>(currentDrawable);
            X11Display::SetGameWindow(win);
            if (!g_inputWired && g_imguiCtx) {
                ImGui::SetCurrentContext(g_imguiCtx);
                X11Input::Install(win);
                ImGui_ImplX11_Init(X11Display::Get(), win);
                X11Input::SetEventCallback(RouteX11EventToImGui);
                g_inputWired = true;
                HOOK_LOG("[Toolscreen] X11 input installed on window 0x%lx\n", win);
            }
        }
    }

    // Render ImGui overlay
    static int g_frameCounter = 0;
    ++g_frameCounter;
    bool shouldLog = (g_frameCounter % 100 == 1);

    if (g_imguiInitialized && g_imguiCtx && X11Display::GetGameWindow() != 0) {
        if (!glXGetCurrentContext()) {
            if (shouldLog) HOOK_LOG("[Toolscreen] Frame %d: no GL context, skipping\n", g_frameCounter);
            inHkSwap = false;
            auto* realSwap = g_realSwapBuffers.load(std::memory_order_acquire);
            if (realSwap) realSwap(dpy, drawable);
            return;
        }

        if (shouldLog) HOOK_LOG("[Toolscreen] Frame %d: rendering ImGui\n", g_frameCounter);
        {
            gloverlay::ScopedState glState;

            // Sodium sets GL_UNPACK_ROW_LENGTH → ImGui font upload reads
            // out-of-bounds → SIGSEGV in NVIDIA driver. Reset before ImGui.
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

            ImGui::SetCurrentContext(g_imguiCtx);
            ImGuiIO& io = ImGui::GetIO();
            if (io.DisplaySize.x <= 0.0f) {
                io.DisplaySize = ImVec2(1920.0f, 1080.0f);
                io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
            }
            X11Input::PollEvents();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplX11_NewFrame();
            ImGui::NewFrame();
            ImGui::Begin("Toolscreen");
            ImGui::Text("Injector OK");
            ImGui::End();
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
        if (shouldLog) HOOK_LOG("[Toolscreen] Frame %d: GL state restored\n", g_frameCounter);
    }

    // Call the real SwapBuffers via saved dispatch table pointer.
    inHkSwap = false;
    auto* realImpl = g_realSwapBuffers.load(std::memory_order_acquire);
    if (realImpl) {
        realImpl(dpy, drawable);
    }
    inHkSwap = false;
}

void hk_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    // Re-entrancy guard (our own viewport calls should not be intercepted)
    static thread_local bool internalCall = false;
    if (internalCall) {
        if (g_realViewport) g_realViewport(x, y, width, height);
        return;
    }

    // TODO: Mode viewport override logic from dllmain.cpp

    if (g_realViewport) {
        internalCall = true;
        g_realViewport(x, y, width, height);
        internalCall = false;
    }
}

void hk_glBindTexture(GLenum target, GLuint texture) {
    // TODO: Track game's framebuffer textures for mirror capture
    if (g_realBindTexture) {
        g_realBindTexture(target, texture);
    }
}

void hk_glBindFramebuffer(GLenum target, GLuint framebuffer) {
    // TODO: Track game FBO for capture
    if (g_realBindFramebuffer) {
        g_realBindFramebuffer(target, framebuffer);
    }
}

void hk_glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                          GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                          GLbitfield mask, GLenum filter) {
    // TODO: OBS redirect (if enabled)
    if (g_realBlitFramebuffer) {
        g_realBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    }
}

SwapBuffersFunc GetRealSwapBuffers() { return g_realSwapBuffers.load(); }
bool IsHooked() { return g_realSwapBuffers.load() != nullptr; }

// Runtime hook for dlopen-based injection.
// Instead of patching code (mprotect on .text crashes JVM), we patch the
// dispatch table pointer that glXSwapBuffers jumps through.
//
// glXSwapBuffers does:  mov offset(%rip),%rax; jmp *0x118(%rax)
// We load the table pointer from the mov instruction, then replace
// table[0x118/8] with our hk_glXSwapBuffers. This is a data-only patch.
void InstallRuntimeHook() {
    TRACE_CALL("[Toolscreen] InstallRuntimeHook: enter\n");
    static std::once_flag s_flag;
    std::call_once(s_flag, []() {
        TRACE_CALL("[Toolscreen] InstallRuntimeHook: call_once body\n");
        void* target = dlsym(RTLD_NEXT, "glXSwapBuffers");
        if (!target) {
            void* libGL = dlopen("libGL.so.1", RTLD_LAZY | RTLD_NOLOAD);
            if (libGL) target = dlsym(libGL, "glXSwapBuffers");
        }
        if (!target) {
            HOOK_LOG("[Toolscreen] InstallRuntimeHook: glXSwapBuffers not found\n");
            return;
        }
        HOOK_LOG("[Toolscreen] glXSwapBuffers at %p\n", target);

        // Parse: mov 0x3f295(%rip),%rax  at target+4
        // Opcode: 48 8B 05 XX XX XX XX (REX.W + MOV + [rip+disp32])
        uint8_t* code = static_cast<uint8_t*>(target);
        if (code[4] != 0x48 || code[5] != 0x8B || code[6] != 0x05) {
            HOOK_LOG("[Toolscreen] Unexpected instruction at glXSwapBuffers+4\n");
            return;
        }
        int32_t disp = 0;
        memcpy(&disp, &code[7], 4);
        void** table_ptr = reinterpret_cast<void**>(code + 4 + 7 + disp);
        void* table = *table_ptr;
        HOOK_LOG("[Toolscreen] Dispatch table at %p (via offset 0x%x)\n", table, disp);

        // Replace table entry for SwapBuffers (offset 0x118)
        const ptrdiff_t kSwapBuffersOffset = 0x118;
        void** slot = reinterpret_cast<void**>(static_cast<uint8_t*>(table) + kSwapBuffersOffset);
        void* origFunc = *slot;

        // Save original for calling
        g_realSwapBuffers.store(reinterpret_cast<SwapBuffersFunc>(origFunc));
        HOOK_LOG("[Toolscreen] Original SwapBuffers impl: %p\n", origFunc);

        // Make table page writable, patch, restore.
        // mprotect on DATA (not code) — safer for JVM.
        size_t pageSize = sysconf(_SC_PAGESIZE);
        void* page = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(slot) & ~(pageSize - 1));
        size_t span = (reinterpret_cast<uintptr_t>(slot) + sizeof(void*) - reinterpret_cast<uintptr_t>(page) + pageSize - 1) & ~(pageSize - 1);
        if (mprotect(page, span, PROT_READ | PROT_WRITE) != 0) {
            HOOK_LOG("[Toolscreen] mprotect(RW) on dispatch table failed: %s\n", strerror(errno));
            return;
        }
        *slot = reinterpret_cast<void*>(hk_glXSwapBuffers);
        mprotect(page, span, PROT_READ);
        HOOK_LOG("[Toolscreen] Dispatch table patched: %p → %p\n", origFunc, *slot);
        TRACE_CALL("[Toolscreen] InstallRuntimeHook: dispatch hook OK\n");
    });
    TRACE_CALL("[Toolscreen] InstallRuntimeHook: exit\n");
}

void CallRealSwapBuffers(Display* dpy, GLXDrawable drawable) {
    auto* realSwap = g_realSwapBuffers.load();
    if (realSwap) {
        realSwap(dpy, drawable);
    }
}

bool Initialize() {
    if (g_initialized.load()) return true;

    HOOK_LOG("[Toolscreen] GLXHook initializing...\n");

    // All GL/GLX functions are intercepted via LD_PRELOAD symbol interposition.
    // The dynamic linker resolves to our exported copies first.
    // Each interposed function resolves the real implementation via
    // dlsym(RTLD_NEXT) + std::call_once on first call.
    //
    // No inline hooks are installed for GL functions — this avoids the
    // torn-read race that would occur if we patched code while the game
    // is already rendering on another thread.

    g_initialized.store(true);
    return true;
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(g_hookMutex);
    for (auto& pair : g_hooks) {
        if (pair.second.bridge) {
            munmap(pair.second.bridge, pair.second.bridgeSize);
        }
        if (pair.second.trampoline) {
            munmap(pair.second.trampoline, pair.second.trampolineSize);
        }
        if (pair.second.enabled) {
            RestoreJump(pair.second.target, pair.second.backup);
        }
    }
    g_hooks.clear();
    g_hooksEnabled.store(false);
}

} // namespace GLXHook

#endif // PLATFORM_LINUX
