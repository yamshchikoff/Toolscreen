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

// ---- Inline hook engine (kept for potential future non-GL use) ----
// Replaces the core MinHook functionality using mprotect + trampolines.
// x86-64 only: installs a 14-byte absolute jump: mov rax, imm64; jmp rax
//
// WARNING: This engine is NOT safe for multithreaded code. The mprotect +
// memcpy sequence can tear instructions while another thread executes the
// same page, causing SIGILL or undefined behaviour. It is deliberately NOT
// used for GL/GLX functions — those are intercepted via LD_PRELOAD symbol
// interposition (see extern "C" blocks below).
// Only use CreateHook/EnableHook for functions known to be single-threaded
// or during process startup before other threads are active.

struct HookEntry {
    void* target;
    void* detour;
    void* trampoline;           // Callable original function
    uint8_t backup[14];         // Original bytes (restored on DisableHook)
    size_t trampolineSize;      // For munmap in Shutdown
    bool enabled;
};

std::mutex g_hookMutex;
std::unordered_map<void*, HookEntry> g_hooks;
std::atomic<bool> g_initialized{false};
std::atomic<bool> g_hooksEnabled{false};
std::atomic<bool> g_glewReady{false};

constexpr size_t kJumpSize = 14; // mov rax, imm64 = 10 bytes; jmp rax = 2 bytes; NOP padding

// Calculate the page-aligned start address containing `addr`
void* PageAlign(void* addr) {
    long pageSize = sysconf(_SC_PAGESIZE);
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(addr) & ~(pageSize - 1));
}

// Calculate the page size spanning [addr, addr + size)
size_t PageSpan(void* addr, size_t size) {
    long pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t start = reinterpret_cast<uintptr_t>(PageAlign(addr));
    uintptr_t end = reinterpret_cast<uintptr_t>(addr) + size;
    end = (end + pageSize - 1) & ~(pageSize - 1);
    return end - start;
}

// Write an absolute 64-bit jump at `target` redirecting to `destination`.
// Backs up original bytes into `backup`.
// Returns the page span that was made writable (caller should restore protection).
void InstallJump(void* target, void* destination, uint8_t* backup) {
    // Back up original bytes
    memcpy(backup, target, kJumpSize);

    // Make page writable
    size_t span = PageSpan(target, kJumpSize);
    if (mprotect(PageAlign(target), span, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        //HOOK_LOG("[Toolscreen] mprotect FAILED for %p: %s\n", target, strerror(errno));
        return;
    }

    // Fill with NOPs
    memset(target, 0x90, kJumpSize);

    // Write: mov rax, imm64; jmp rax
    uint8_t* code = static_cast<uint8_t*>(target);
    code[0] = 0x48; // REX.W
    code[1] = 0xB8; // MOV RAX, imm64
    memcpy(&code[2], &destination, sizeof(void*));
    code[10] = 0xFF; // JMP RAX
    code[11] = 0xE0;

    // Restore page protection
    mprotect(PageAlign(target), span, PROT_READ | PROT_EXEC);
}

// Write the original bytes back to `target` (reverse of InstallJump).
void RestoreJump(void* target, const uint8_t* backup) {
    size_t span = PageSpan(target, kJumpSize);
    if (mprotect(PageAlign(target), span, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        //HOOK_LOG("[Toolscreen] mprotect RWX (restore) failed for %p: %s\n", target, strerror(errno));
        return;
    }
    memcpy(target, backup, kJumpSize);
    mprotect(PageAlign(target), span, PROT_READ | PROT_EXEC);
}

// Create a trampoline that executes the backed-up bytes then jumps to (target + kJumpSize)
void* CreateTrampoline(void* target, const uint8_t* backup) {
    // Allocate executable memory for trampoline
    size_t trampSize = kJumpSize + kJumpSize; // backup bytes + another jump
    void* tramp = mmap(nullptr, trampSize, PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (tramp == MAP_FAILED) {
        //HOOK_LOG("[Toolscreen] mmap trampoline failed\n");
        return nullptr;
    }

    // Copy original bytes
    memcpy(tramp, backup, kJumpSize);

    // Append jump to original function (after the backup bytes)
    uint8_t* code = static_cast<uint8_t*>(tramp) + kJumpSize;
    uintptr_t nextAddr = reinterpret_cast<uintptr_t>(target) + kJumpSize;
    code[0] = 0x48;
    code[1] = 0xB8;
    memcpy(&code[2], &nextAddr, sizeof(void*));
    code[10] = 0xFF;
    code[11] = 0xE0;

    mprotect(tramp, trampSize, PROT_READ | PROT_EXEC);
    return tramp;
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
        // Already hooked — return existing trampoline
        if (original) *original = it->second.trampoline;
        return true;
    }

    HookEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.target = target;
    entry.detour = detour;
    entry.trampolineSize = kJumpSize + kJumpSize; // backup + jump
    entry.enabled = false;

    // Install the jump from target to detour
    InstallJump(target, detour, entry.backup);

    // Create trampoline
    entry.trampoline = CreateTrampoline(target, entry.backup);
    if (!entry.trampoline) {
        // Restore original bytes on failure
        RestoreJump(target, entry.backup);
        return false;
    }

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

    // Recursion guard: if we called realSwap, it calls the original
    // glXSwapBuffers which should NOT re-enter our hook.
    static thread_local bool inHkSwap = false;
    if (inHkSwap) {
        static SwapBuffersFunc s_real = nullptr;
        if (!s_real) s_real = reinterpret_cast<SwapBuffersFunc>(dlsym(RTLD_NEXT, "glXSwapBuffers"));
        if (s_real) s_real(dpy, drawable);
        return;
    }
    inHkSwap = true;

    // Deferred initialization — avoids conflicts with Java classloaders
    ToolscreenLazyInit();

    // Lazy GLEW initialization on first call (requires an active GL context,
    // so we must do it here — not in Initialize()).
    // Detect GL context change (e.g. fullscreen ↔ windowed toggle in Minecraft)
    // and reinitialize GLEW so function pointers point to the current context.
    // NOTE: Minecraft calls glXSwapBuffers from a single render thread,
    // so this path is not contended. s_glewContext is still atomic as a safeguard.
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
        GLenum glewErr = glewInit();
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

    // ---- Create ImGui context BEFORE window detection ----
    // (ImGui_ImplX11_Init calls ImGui::GetIO() which needs a context)
    // Protected by std::call_once — safe against concurrent first calls
    // even if the render thread were to race (belt and suspenders).
    static std::once_flag g_imguiInitFlag;
    static std::atomic<bool> g_imguiInitialized{false};
    static ImGuiContext* g_imguiCtx = nullptr;
    if (!g_imguiInitialized.load(std::memory_order_acquire) && g_glewReady.load()) {
        std::call_once(g_imguiInitFlag, [&]() {
            // ImGui_ImplOpenGL3_Init creates shaders, VAO, fonts texture —
            // it dirties GL state (active program, texture binding, etc.).
            // Save/restore around Init so the game's GL state is preserved
            // BEFORE the first ScopedState in the render block below.
            gloverlay::ScopedState initState;
            IMGUI_CHECKVERSION();
            g_imguiCtx = ImGui::CreateContext();
            ImGui::SetCurrentContext(g_imguiCtx);
            ImGui_ImplOpenGL3_Init("#version 330");
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            ImGui::GetStyle().FrameRounding = 3.0f;
            g_imguiInitialized.store(true, std::memory_order_release);
            HOOK_LOG("[Toolscreen] ImGui initialized (X11/OpenGL3)\n");
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

    // ---- Render GUI (context already created above) ----
    // ---- Render ImGui overlay ----
    // Debug frame counter (log every 100th frame)
    static int g_frameCounter = 0;
    ++g_frameCounter;
    bool shouldLog = (g_frameCounter % 100 == 1);

    if (g_imguiInitialized && g_imguiCtx && X11Display::GetGameWindow() != 0) {
        // Verify GL context is still alive before touching GL state
        if (!glXGetCurrentContext()) {
            if (shouldLog) HOOK_LOG("[Toolscreen] Frame %d: no GL context, skipping render\n", g_frameCounter);
            // Skip this frame — no GL context to render into
            inHkSwap = false;
            static SwapBuffersFunc s_fallbackSwap = nullptr;
            if (!s_fallbackSwap) s_fallbackSwap = reinterpret_cast<SwapBuffersFunc>(dlsym(RTLD_NEXT, "glXSwapBuffers"));
            if (s_fallbackSwap) s_fallbackSwap(dpy, drawable);
            return;
        }

        // Save & restore GL state around ImGui rendering.
        // ImGui_ImplOpenGL3_RenderDrawData internally saves/restores blend, scissor,
        // depth, cull, stencil, viewport, program, VAO, textures — but does NOT
        // restore GL_DRAW_FRAMEBUFFER_BINDING. Sodium renders into custom FBOs;
        // leaving the FBO unbound after ImGui crashes the NVIDIA driver.
        if (shouldLog) HOOK_LOG("[Toolscreen] Frame %d: saving GL state, rendering ImGui\n", g_frameCounter);
        {
            gloverlay::ScopedState glState;

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
            static bool showDemo = true;
            ImGui::ShowDemoWindow(&showDemo);
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
        if (shouldLog) HOOK_LOG("[Toolscreen] Frame %d: GL state restored\n", g_frameCounter);
    }

    // Call the real glXSwapBuffers via RTLD_NEXT (not trampoline!)
    static SwapBuffersFunc s_realSwap = nullptr;
    if (!s_realSwap) {
        s_realSwap = reinterpret_cast<SwapBuffersFunc>(dlsym(RTLD_NEXT, "glXSwapBuffers"));
    }
    inHkSwap = false;
    if (s_realSwap) s_realSwap(dpy, drawable);
    // Reset recursion guard for next frame
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
// Called from the constructor. Uses the existing inline hook engine to
// redirect glXSwapBuffers → hk_glXSwapBuffers in the already-running process.
void InstallRuntimeHook() {
    static std::once_flag s_flag;
    std::call_once(s_flag, []() {
        // RTLD_NEXT skips our own .so and finds the real libGL's glXSwapBuffers
        void* target = dlsym(RTLD_NEXT, "glXSwapBuffers");
        if (!target) {
            void* libGL = dlopen("libGL.so.1", RTLD_LAZY | RTLD_NOLOAD);
            if (libGL) target = dlsym(libGL, "glXSwapBuffers");
        }
        if (!target) {
            HOOK_LOG("[Toolscreen] InstallRuntimeHook: glXSwapBuffers not found\n");
            return;
        }
        void* trampoline = nullptr;
        if (CreateHook(target, reinterpret_cast<void*>(hk_glXSwapBuffers), &trampoline) && trampoline) {
            // Store the trampoline (original glXSwapBuffers) for Shutdown/CallRealSwapBuffers
            g_realSwapBuffers.store(reinterpret_cast<SwapBuffersFunc>(trampoline));
            HOOK_LOG("[Toolscreen] glXSwapBuffers hooked at %p\n", target);
        } else {
            HOOK_LOG("[Toolscreen] InstallRuntimeHook: CreateHook failed\n");
        }
    });
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
    // Free trampoline allocations
    std::lock_guard<std::mutex> lock(g_hookMutex);
    for (auto& pair : g_hooks) {
        if (pair.second.trampoline) {
            munmap(pair.second.trampoline, pair.second.trampolineSize);
        }
        // Restore original bytes if hook is still enabled
        if (pair.second.enabled) {
            RestoreJump(pair.second.target, pair.second.backup);
        }
    }
    g_hooks.clear();
    g_hooksEnabled.store(false);
}

} // namespace GLXHook

#endif // PLATFORM_LINUX
