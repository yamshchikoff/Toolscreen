#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // For RTLD_NEXT
#endif

#include "glx_hook.h"
#include "x11_display.h"
#include "common/profiler.h"

#ifdef PLATFORM_LINUX

#include <GL/glew.h>
#include <GL/glx.h>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace GLXHook {

// ---- Original function pointers ----
std::atomic<SwapBuffersFunc> g_realSwapBuffers{nullptr};
ViewportFunc g_realViewport = nullptr;
BindTextureFunc g_realBindTexture = nullptr;
BindFramebufferFunc g_realBindFramebuffer = nullptr;
BlitFramebufferFunc g_realBlitFramebuffer = nullptr;

// ---- GLXSwapBuffers interposition via LD_PRELOAD ----
// On Linux, we use LD_PRELOAD symbol interposition for glXSwapBuffers.
// Our .so exports glXSwapBuffers, and the dynamic linker resolves to our copy first.
// We call the real one via dlsym(RTLD_NEXT, ...).
// For other GL functions (resolved via glXGetProcAddress or GLEW), we use inline hooking.

namespace {

// ---- Inline hook engine ----
// Replaces the core MinHook functionality using mprotect + trampolines.
// x86-64 only: installs a 14-byte absolute jump: mov rax, imm64; jmp rax

struct HookEntry {
    void* target;
    void* detour;
    void* trampoline; // Callable original function
    size_t originalSize;
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

// Install the absolute jump at `target` to redirect to `destination`
void InstallJump(void* target, void* destination, uint8_t* backup, size_t backupSize) {
    // Back up original bytes
    memcpy(backup, target, backupSize);

    // Make page writable
    size_t span = PageSpan(target, kJumpSize);
    if (mprotect(PageAlign(target), span, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        fprintf(stderr, "[Toolscreen] mprotect RWX failed for %p: %s\n", target, strerror(errno));
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

// Create a trampoline that executes the backed-up bytes then jumps to (target + backupSize)
void* CreateTrampoline(void* target, const uint8_t* backup, size_t backupSize) {
    // Allocate executable memory for trampoline
    size_t trampSize = backupSize + kJumpSize;
    void* tramp = mmap(nullptr, trampSize, PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (tramp == MAP_FAILED) {
        fprintf(stderr, "[Toolscreen] mmap trampoline failed\n");
        return nullptr;
    }

    // Copy original bytes
    memcpy(tramp, backup, backupSize);

    // Append jump to original function (after the backup bytes)
    uint8_t* code = static_cast<uint8_t*>(tramp) + backupSize;
    uintptr_t nextAddr = reinterpret_cast<uintptr_t>(target) + backupSize;
    code[0] = 0x48;
    code[1] = 0xB8;
    memcpy(&code[2], &nextAddr, sizeof(void*));
    code[10] = 0xFF;
    code[11] = 0xE0;

    mprotect(tramp, trampSize, PROT_READ | PROT_EXEC);
    return tramp;
}

} // namespace

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
    entry.target = target;
    entry.detour = detour;
    entry.originalSize = kJumpSize;
    entry.enabled = false;

    // Store backup bytes for this specific hook (BUGFIX: was static, now local)
    uint8_t backup[kJumpSize] = {};

    // Install the jump from target to detour
    InstallJump(target, detour, backup, kJumpSize);

    // Create trampoline
    entry.trampoline = CreateTrampoline(target, backup, kJumpSize);
    if (!entry.trampoline) {
        // Restore original bytes on failure
        size_t span = PageSpan(target, kJumpSize);
        mprotect(PageAlign(target), span, PROT_READ | PROT_WRITE | PROT_EXEC);
        memcpy(target, backup, kJumpSize);
        mprotect(PageAlign(target), span, PROT_READ | PROT_EXEC);
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
    it->second.enabled = true;
    return true;
}

bool DisableHook(void* hook) {
    std::lock_guard<std::mutex> lock(g_hookMutex);
    auto it = g_hooks.find(hook);
    if (it == g_hooks.end()) return false;
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

// ---- GLX SwapBuffers interposition ----
// This function OVERRIDES the system glXSwapBuffers via LD_PRELOAD symbol interposition.
// The dynamic linker calls our version instead of the one in libGL.
// We use dlsym(RTLD_NEXT, ...) to find and call the real implementation.

extern "C" {

// Declare our own glXSwapBuffers that overrides the system one
void glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
    // Re-entrancy guard
    static thread_local int swapDepth = 0;
    if (swapDepth > 0) {
        // If we're already inside our swap handler and something calls swap again,
        // go directly to the real implementation
        if (g_realSwapBuffers.load()) {
            g_realSwapBuffers.load()(dpy, drawable);
        }
        return;
    }
    ++swapDepth;

    // Resolve the real glXSwapBuffers on first call
    if (!g_realSwapBuffers.load()) {
        g_realSwapBuffers.store(reinterpret_cast<SwapBuffersFunc>(
            dlsym(RTLD_NEXT, "glXSwapBuffers")));
        if (!g_realSwapBuffers.load()) {
            fprintf(stderr, "[Toolscreen] FATAL: Cannot find real glXSwapBuffers\n");
            --swapDepth;
            return;
        }
    }

    // IMPORTANT: Render overlays BEFORE the real swap so they appear
    // on the current frame (matching Windows SwapBuffers hook behavior).
    // On Windows: RenderMode() → next(hDc) [real swap]
    // On Linux:  RenderMode() → real_glXSwapBuffers
    hk_glXSwapBuffers(dpy, drawable);

    // Call the real glXSwapBuffers (game frame is presented with our overlays)
    g_realSwapBuffers.load()(dpy, drawable);

    --swapDepth;
}

} // extern "C"

// ---- Hooked function implementations ----

void hk_glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
    PROFILE_SCOPE("glXSwapBuffers");

    // Lazy GLEW initialization on first call
    if (!g_glewReady.load(std::memory_order_acquire)) {
        GLenum glewErr = glewInit();
        if (glewErr == GLEW_OK) {
            g_glewReady.store(true, std::memory_order_release);
            fprintf(stderr, "[Toolscreen] GLEW initialized OK\n");

            // Install additional GL hooks now that GLEW is ready
            void* viewportFunc = GetGLFunc("glViewport");
            if (viewportFunc) {
                CreateHook(viewportFunc, reinterpret_cast<void*>(hk_glViewport),
                          reinterpret_cast<void**>(&g_realViewport));
            }

            void* bindTexFunc = GetGLFunc("glBindTexture");
            if (bindTexFunc) {
                CreateHook(bindTexFunc, reinterpret_cast<void*>(hk_glBindTexture),
                          reinterpret_cast<void**>(&g_realBindTexture));
            }

            void* bindFbFunc = GetGLFunc("glBindFramebuffer");
            if (bindFbFunc) {
                CreateHook(bindFbFunc, reinterpret_cast<void*>(hk_glBindFramebuffer),
                          reinterpret_cast<void**>(&g_realBindFramebuffer));
            }

            void* blitFbFunc = GetGLFunc("glBlitFramebuffer");
            if (blitFbFunc) {
                CreateHook(blitFbFunc, reinterpret_cast<void*>(hk_glBlitFramebuffer),
                          reinterpret_cast<void**>(&g_realBlitFramebuffer));
            }
        } else {
            fprintf(stderr, "[Toolscreen] GLEW init failed: %s\n",
                    reinterpret_cast<const char*>(glewGetErrorString(glewErr)));
        }
    }

    // Detect game window from current GLX drawable
    if (X11Display::GetGameWindow() == 0) {
        Window win = 0;
        // glXGetCurrentDrawable returns the current GLXDrawable
        // For on-screen rendering, this is a Window (X11 Window ID)
        GLXDrawable currentDrawable = glXGetCurrentDrawable();
        if (currentDrawable) {
            win = static_cast<Window>(currentDrawable);
            X11Display::SetGameWindow(win);

            // Note: XSelectInput is handled by X11Input::Install()
            // to avoid overwriting the game's existing event mask.
        }
    }

    // TODO: Full render pipeline — RenderMode(), GUI, etc.
    // This will be connected in later phases when we integrate the ported subsystems.
}

void hk_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    // Re-entrancy guard (our own viewport calls should not be intercepted)
    static thread_local bool internalCall = false;
    if (internalCall) {
        if (g_realViewport) g_realViewport(x, y, width, height);
        return;
    }

    // TODO: Mode viewport override logic from dllmain.cpp
    // This modifies the viewport for the mode system (fullscreen, thin, wide, etc.)
    // and EyeZoom magnification.

    if (g_realViewport) {
        g_realViewport(x, y, width, height);
    }
}

void hk_glBindTexture(GLenum target, GLuint texture) {
    // TODO: Track game's framebuffer textures for mirror capture
    // Same logic as dllmain.cpp glBindTexture hook
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
    // Same logic as dllmain.cpp / obs_thread.cpp
    if (g_realBlitFramebuffer) {
        g_realBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    }
}

SwapBuffersFunc GetRealSwapBuffers() { return g_realSwapBuffers.load(); }
bool IsHooked() { return g_realSwapBuffers.load() != nullptr; }

void CallRealSwapBuffers(Display* dpy, GLXDrawable drawable) {
    auto* realSwap = g_realSwapBuffers.load();
    if (realSwap) {
        realSwap(dpy, drawable);
    }
}

bool Initialize() {
    if (g_initialized.load()) return true;

    fprintf(stderr, "[Toolscreen] GLXHook initializing...\n");

    // We don't need to hook glXSwapBuffers — it's intercepted via LD_PRELOAD
    // (our .so exports glXSwapBuffers, we call the real one via dlsym(RTLD_NEXT))

    // For other GL functions, we'll install hooks after GLEW init
    g_initialized.store(true);
    return true;
}

void Shutdown() {
    // Restore all inline hooks
    std::lock_guard<std::mutex> lock(g_hookMutex);
    // Note: Inline hooks are not easily restorable without backing up original bytes.
    // For now, we just let the process exit (hooks die with the process).
    g_hooksEnabled.store(false);
}

} // namespace GLXHook

#endif // PLATFORM_LINUX
