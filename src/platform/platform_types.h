#pragma once

// Platform-neutral type definitions for Toolscreen.
// Replaces <windows.h> dependency across the codebase.

#include <cstdint>
#include <string>

// ---- Basic platform-neutral integral types ----
using u32 = uint32_t;
using u64 = uint64_t;
using s32 = int32_t;
using s64 = int64_t;

// ---- Virtual key code (matches both Win32 VK_* and Linux evdev/X11 keycodes where possible) ----
using PlatformVk = uint32_t;

// ---- Window handle ----
struct PlatformWindow; // opaque
using PlatformWindowHandle = PlatformWindow*;

// ---- Display/device context ----
struct PlatformDisplay; // opaque
using PlatformDisplayHandle = PlatformDisplay*;

// ---- OpenGL context ----
using PlatformGLContext = void*;

// ---- Module/library handle ----
using PlatformModuleHandle = void*;

// ---- Cursor handle ----
using PlatformCursorHandle = void*;

// ---- Thread ID ----
using PlatformThreadId = uint64_t;

// ---- Generic 2D rectangle ----
struct PlatformRect {
    s32 left;
    s32 top;
    s32 right;
    s32 bottom;

    s32 width()  const { return right - left; }
    s32 height() const { return bottom - top; }
};

// ---- Monitor info ----
struct PlatformMonitorInfo {
    PlatformRect rect;
    bool isPrimary;
};

// ---- Virtual key code constants (shared between Windows and Linux mappings) ----
// We use the Windows VK_* values as the canonical representation and map
// Linux keycodes to them. This preserves config file compatibility.
namespace Vk {
    inline constexpr PlatformVk NONE       = 0x00;
    inline constexpr PlatformVk LBUTTON    = 0x01;
    inline constexpr PlatformVk RBUTTON    = 0x02;
    inline constexpr PlatformVk CANCEL     = 0x03;
    inline constexpr PlatformVk MBUTTON    = 0x04;
    inline constexpr PlatformVk XBUTTON1   = 0x05;
    inline constexpr PlatformVk XBUTTON2   = 0x06;
    inline constexpr PlatformVk BACK       = 0x08;
    inline constexpr PlatformVk TAB        = 0x09;
    inline constexpr PlatformVk CLEAR      = 0x0C;
    inline constexpr PlatformVk RETURN     = 0x0D;
    inline constexpr PlatformVk SHIFT      = 0x10;
    inline constexpr PlatformVk CONTROL    = 0x11;
    inline constexpr PlatformVk MENU       = 0x12;
    inline constexpr PlatformVk PAUSE      = 0x13;
    inline constexpr PlatformVk CAPITAL    = 0x14;
    inline constexpr PlatformVk ESCAPE     = 0x1B;
    inline constexpr PlatformVk SPACE      = 0x20;
    inline constexpr PlatformVk PRIOR      = 0x21;
    inline constexpr PlatformVk NEXT       = 0x22;
    inline constexpr PlatformVk END        = 0x23;
    inline constexpr PlatformVk HOME       = 0x24;
    inline constexpr PlatformVk LEFT       = 0x25;
    inline constexpr PlatformVk UP         = 0x26;
    inline constexpr PlatformVk RIGHT      = 0x27;
    inline constexpr PlatformVk DOWN       = 0x28;
    inline constexpr PlatformVk SELECT     = 0x29;
    inline constexpr PlatformVk PRINT      = 0x2A;
    inline constexpr PlatformVk EXECUTE    = 0x2B;
    inline constexpr PlatformVk SNAPSHOT   = 0x2C;
    inline constexpr PlatformVk INSERT     = 0x2D;
    inline constexpr PlatformVk DELETE     = 0x2E;
    inline constexpr PlatformVk HELP       = 0x2F;
    inline constexpr PlatformVk LWIN       = 0x5B;
    inline constexpr PlatformVk RWIN       = 0x5C;
    inline constexpr PlatformVk APPS       = 0x5D;
    inline constexpr PlatformVk SLEEP      = 0x5F;

    // Digit keys (0x30-0x39 map to '0'-'9' in ASCII)
    inline constexpr PlatformVk KEY_0 = 0x30; inline constexpr PlatformVk KEY_1 = 0x31;
    inline constexpr PlatformVk KEY_2 = 0x32; inline constexpr PlatformVk KEY_3 = 0x33;
    inline constexpr PlatformVk KEY_4 = 0x34; inline constexpr PlatformVk KEY_5 = 0x35;
    inline constexpr PlatformVk KEY_6 = 0x36; inline constexpr PlatformVk KEY_7 = 0x37;
    inline constexpr PlatformVk KEY_8 = 0x38; inline constexpr PlatformVk KEY_9 = 0x39;

    // Letter keys (0x41-0x5A map to 'A'-'Z')
    inline constexpr PlatformVk KEY_A = 0x41; inline constexpr PlatformVk KEY_B = 0x42;
    inline constexpr PlatformVk KEY_C = 0x43; inline constexpr PlatformVk KEY_D = 0x44;
    inline constexpr PlatformVk KEY_E = 0x45; inline constexpr PlatformVk KEY_F = 0x46;
    inline constexpr PlatformVk KEY_G = 0x47; inline constexpr PlatformVk KEY_H = 0x48;
    inline constexpr PlatformVk KEY_I = 0x49; inline constexpr PlatformVk KEY_J = 0x4A;
    inline constexpr PlatformVk KEY_K = 0x4B; inline constexpr PlatformVk KEY_L = 0x4C;
    inline constexpr PlatformVk KEY_M = 0x4D; inline constexpr PlatformVk KEY_N = 0x4E;
    inline constexpr PlatformVk KEY_O = 0x4F; inline constexpr PlatformVk KEY_P = 0x50;
    inline constexpr PlatformVk KEY_Q = 0x51; inline constexpr PlatformVk KEY_R = 0x52;
    inline constexpr PlatformVk KEY_S = 0x53; inline constexpr PlatformVk KEY_T = 0x54;
    inline constexpr PlatformVk KEY_U = 0x55; inline constexpr PlatformVk KEY_V = 0x56;
    inline constexpr PlatformVk KEY_W = 0x57; inline constexpr PlatformVk KEY_X = 0x58;
    inline constexpr PlatformVk KEY_Y = 0x59; inline constexpr PlatformVk KEY_Z = 0x5A;

    inline constexpr PlatformVk NUMPAD0    = 0x60;
    inline constexpr PlatformVk NUMPAD1    = 0x61;
    inline constexpr PlatformVk NUMPAD2    = 0x62;
    inline constexpr PlatformVk NUMPAD3    = 0x63;
    inline constexpr PlatformVk NUMPAD4    = 0x64;
    inline constexpr PlatformVk NUMPAD5    = 0x65;
    inline constexpr PlatformVk NUMPAD6    = 0x66;
    inline constexpr PlatformVk NUMPAD7    = 0x67;
    inline constexpr PlatformVk NUMPAD8    = 0x68;
    inline constexpr PlatformVk NUMPAD9    = 0x69;
    inline constexpr PlatformVk MULTIPLY   = 0x6A;
    inline constexpr PlatformVk ADD        = 0x6B;
    inline constexpr PlatformVk SEPARATOR  = 0x6C;
    inline constexpr PlatformVk SUBTRACT   = 0x6D;
    inline constexpr PlatformVk DECIMAL    = 0x6E;
    inline constexpr PlatformVk DIVIDE     = 0x6F;
    inline constexpr PlatformVk F1         = 0x70;
    inline constexpr PlatformVk F2         = 0x71;
    inline constexpr PlatformVk F3         = 0x72;
    inline constexpr PlatformVk F4         = 0x73;
    inline constexpr PlatformVk F5         = 0x74;
    inline constexpr PlatformVk F6         = 0x75;
    inline constexpr PlatformVk F7         = 0x76;
    inline constexpr PlatformVk F8         = 0x77;
    inline constexpr PlatformVk F9         = 0x78;
    inline constexpr PlatformVk F10        = 0x79;
    inline constexpr PlatformVk F11        = 0x7A;
    inline constexpr PlatformVk F12        = 0x7B;
    inline constexpr PlatformVk F13        = 0x7C;
    inline constexpr PlatformVk F14        = 0x7D;
    inline constexpr PlatformVk F15        = 0x7E;
    inline constexpr PlatformVk F16        = 0x7F;
    inline constexpr PlatformVk NUMLOCK    = 0x90;
    inline constexpr PlatformVk SCROLL     = 0x91;
    inline constexpr PlatformVk LSHIFT     = 0xA0;
    inline constexpr PlatformVk RSHIFT     = 0xA1;
    inline constexpr PlatformVk LCONTROL   = 0xA2;
    inline constexpr PlatformVk RCONTROL   = 0xA3;
    inline constexpr PlatformVk LMENU      = 0xA4;
    inline constexpr PlatformVk RMENU      = 0xA5;
    inline constexpr PlatformVk VOLUME_MUTE    = 0xAD;
    inline constexpr PlatformVk VOLUME_DOWN    = 0xAE;
    inline constexpr PlatformVk VOLUME_UP      = 0xAF;
    inline constexpr PlatformVk MEDIA_NEXT     = 0xB0;
    inline constexpr PlatformVk MEDIA_PREV     = 0xB1;
    inline constexpr PlatformVk MEDIA_STOP     = 0xB2;
    inline constexpr PlatformVk MEDIA_PLAY_PAUSE = 0xB3;
    inline constexpr PlatformVk OEM_1       = 0xBA; // ;:
    inline constexpr PlatformVk OEM_PLUS    = 0xBB; // =+
    inline constexpr PlatformVk OEM_COMMA   = 0xBC; // ,<
    inline constexpr PlatformVk OEM_MINUS   = 0xBD; // -_
    inline constexpr PlatformVk OEM_PERIOD  = 0xBE; // .>
    inline constexpr PlatformVk OEM_2       = 0xBF; // /?
    inline constexpr PlatformVk OEM_3       = 0xC0; // `~
    inline constexpr PlatformVk OEM_4       = 0xDB; // [{
    inline constexpr PlatformVk OEM_5       = 0xDC; // \|
    inline constexpr PlatformVk OEM_6       = 0xDD; // ]}
    inline constexpr PlatformVk OEM_7       = 0xDE; // '"
    inline constexpr PlatformVk OEM_8       = 0xDF;
    inline constexpr PlatformVk OEM_102     = 0xE2;

    // Custom Toolscreen virtual keys
    inline constexpr PlatformVk TOOLSCREEN_SCROLL_UP   = 0x1000;
    inline constexpr PlatformVk TOOLSCREEN_SCROLL_DOWN = 0x1001;
    inline constexpr PlatformVk TOOLSCREEN_SCROLL_LEFT = 0x1002;
    inline constexpr PlatformVk TOOLSCREEN_SCROLL_RIGHT = 0x1003;
    inline constexpr PlatformVk TOOLSCREEN_XBUTTON3    = 0x1004;
    inline constexpr PlatformVk TOOLSCREEN_XBUTTON4    = 0x1005;
    inline constexpr PlatformVk TOOLSCREEN_XBUTTON5    = 0x1006;
    inline constexpr PlatformVk TOOLSCREEN_XBUTTON6    = 0x1007;

    // Mouse button aliases
    inline constexpr PlatformVk MOUSE_LEFT   = LBUTTON;
    inline constexpr PlatformVk MOUSE_RIGHT  = RBUTTON;
    inline constexpr PlatformVk MOUSE_MIDDLE = MBUTTON;
}

// ---- Platform-native type aliases (for code that still needs platform-specific types) ----
#ifdef __linux__
    // On Linux, we use our own types
    using NativeWindow = PlatformWindowHandle;
    using NativeDisplay = PlatformDisplayHandle;
    using NativeCursor = PlatformCursorHandle;
    using NativeRect = PlatformRect;
    using NativeModule = PlatformModuleHandle;
    using NativeThreadId = uint64_t;

    #define PLATFORM_LINUX 1
#elif defined(_WIN32)
    // On Windows, these map to the real Win32 types
    #include <windows.h>
    using NativeWindow = HWND;
    using NativeDisplay = HDC;
    using NativeCursor = HCURSOR;
    using NativeRect = RECT;
    using NativeModule = HMODULE;
    using NativeThreadId = DWORD;

    #define PLATFORM_WINDOWS 1
#else
    #error "Unsupported platform"
#endif

// ---- Key code conversion (platform-specific implementations) ----
// Maps platform-native keycodes to canonical VK_* values
PlatformVk PlatformKeyToVk(uint32_t nativeKeycode);
uint32_t VkToPlatformKey(PlatformVk vk);
std::string VkToString(PlatformVk vk);
PlatformVk StringToVk(const std::string& keyStr);

// ---- Platform services (implemented per-platform) ----
namespace Platform {

// Initialize platform services (called at startup)
bool Init();

// Shutdown platform services (called at unload)
void Shutdown();

// Get the path of the current module/shared library
std::string GetModulePath();

// Get the directory containing the current module
std::string GetModuleDirectory();

// Get current thread ID
PlatformThreadId GetCurrentThreadId();

// Sleep for specified milliseconds
void SleepMs(uint32_t ms);

// Get current time in microseconds (monotonic clock)
uint64_t GetTimeUs();

// Get current time in milliseconds (monotonic clock)
uint64_t GetTimeMs();

// Get the executable path for the current process
std::string GetProcessPath();

// Get the process ID
uint32_t GetProcessId();

// Create a thread (returns platform thread handle)
void* CreateThread(void (*func)(void*), void* arg);

// Wait for a thread to finish
void JoinThread(void* threadHandle);

// Get display/monitor info
int GetMonitorCount();
bool GetMonitorRect(int monitorIndex, PlatformRect& outRect);
bool GetMonitorSize(int monitorIndex, int& outW, int& outH);

} // namespace Platform
