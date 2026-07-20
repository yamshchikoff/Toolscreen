#pragma once

#include "platform/platform_types.h"

#ifdef PLATFORM_LINUX

#include <GL/glx.h>
#include <atomic>

// ---- GLX SwapBuffers hook and GL function interception ----

namespace GLXHook {

// Initialize the hook engine and install all GL hooks
bool Initialize();

// Shutdown and restore all hooks
void Shutdown();

// Get the real (next) glXSwapBuffers to call after our rendering
using SwapBuffersFunc = void (*)(Display*, GLXDrawable);
SwapBuffersFunc GetRealSwapBuffers();

// Call the original glXSwapBuffers (chains to real implementation)
void CallRealSwapBuffers(Display* dpy, GLXDrawable drawable);

// Check if hooks are active
bool IsHooked();

// ---- Inline hooking engine (replaces MinHook on Linux) ----

// Install an inline hook at `target` redirecting to `detour`.
// `original` receives a trampoline that can call the original function.
// Returns true on success.
bool CreateHook(void* target, void* detour, void** original);

// Enable a previously created hook
bool EnableHook(void* hook);

// Disable a hook
bool DisableHook(void* hook);

// Enable all created hooks
bool EnableAllHooks();

// Resolve a GL function by name (wraps glXGetProcAddress + dlsym fallback)
void* GetGLFunc(const char* name);

// ---- Hooked function declarations ----

// These are the detour functions that get installed over the real GL/GLX functions.
// They follow the naming convention hk<OriginalName>.

// glXSwapBuffers hook — main render entry point
void hk_glXSwapBuffers(Display* dpy, GLXDrawable drawable);

// glViewport hook — viewport manipulation for mode system and EyeZoom
void hk_glViewport(GLint x, GLint y, GLsizei width, GLsizei height);

// glBindTexture hook — tracks which texture is the game's framebuffer
void hk_glBindTexture(GLenum target, GLuint texture);

// glBindFramebuffer hook — detects game FBO changes
void hk_glBindFramebuffer(GLenum target, GLuint framebuffer);

// glBlitFramebuffer hook — OBS capture redirect
void hk_glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                          GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                          GLbitfield mask, GLenum filter);

// Store original function pointers
extern std::atomic<SwapBuffersFunc> g_realSwapBuffers;

using ViewportFunc = void (*)(GLint, GLint, GLsizei, GLsizei);
using BindTextureFunc = void (*)(GLenum, GLuint);
using BindFramebufferFunc = void (*)(GLenum, GLuint);
using BlitFramebufferFunc = void (*)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);

extern ViewportFunc g_realViewport;
extern BindTextureFunc g_realBindTexture;
extern BindFramebufferFunc g_realBindFramebuffer;
extern BlitFramebufferFunc g_realBlitFramebuffer;

} // namespace GLXHook

#endif // PLATFORM_LINUX
