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
    inline constexpr PlatformVk F17        = 0x80;
    inline constexpr PlatformVk F18        = 0x81;
    inline constexpr PlatformVk F19        = 0x82;
    inline constexpr PlatformVk F20        = 0x83;
    inline constexpr PlatformVk F21        = 0x84;
    inline constexpr PlatformVk F22        = 0x85;
    inline constexpr PlatformVk F23        = 0x86;
    inline constexpr PlatformVk F24        = 0x87;
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

    // Linux-specific includes needed by polyfills below
    #include <unistd.h>
    #include <sys/syscall.h>
    #include <cstdarg>
    #include <cstring>
    #include <fstream>
    #include <chrono>
    #include <climits>
    #include <codecvt>
    #include <locale>

    // ---- Windows-compatible type shims (shared codebase compatibility) ----
    // Basic types
    using DWORD = uint32_t;
    using UINT = unsigned int;
    using WPARAM = uint64_t;
    using LPARAM = int64_t;
    using LRESULT = int64_t;
    using LPVOID = void*;
    using ULONG_PTR = uintptr_t;
    using WORD = uint16_t;
    using SHORT = int16_t;
    using USHORT = uint16_t;
    using BOOL = int;
    using LONGLONG = int64_t;
    using ULONGLONG = uint64_t;
    #define FALSE 0
    #define TRUE 1

    // Calling-convention macros (no-op on Linux)
    #define WINAPI
    #define APIENTRY
    #define CALLBACK
    #define __stdcall

    // Window/graphics types as opaque pointers
    using HWND = void*;
    using HMODULE = void*;
    using HGLRC = void*;
    using HDC = void*;
    using HCURSOR = void*;
    using WNDPROC = void*;
    using HINSTANCE = void*;
    using HANDLE = void*;
    using HICON = void*;
    using HBRUSH = void*;
    using HMENU = void*;
    using HRGN = void*;
    using HPEN = void*;
    using HFONT = void*;
    using HBITMAP = void*;
    using HRSRC = void*;
    using HGLOBAL = void*;

    // RECT (matching Windows layout)
    struct tagRECT { int32_t left, top, right, bottom; };
    using RECT = tagRECT;

    // SEH types (stubs for Linux — SEH doesn't exist, only on Windows)
    struct _EXCEPTION_RECORD { unsigned long ExceptionCode; unsigned long ExceptionFlags; void* ExceptionRecord; void* ExceptionAddress; unsigned long NumberParameters; unsigned long ExceptionInformation[15]; };
    struct _CONTEXT { unsigned long placeholder[256]; };
    struct EXCEPTION_POINTERS { _EXCEPTION_RECORD* ExceptionRecord; _CONTEXT* ContextRecord; };
    using LONG = int32_t;
    #define EXCEPTION_ACCESS_VIOLATION 0xC0000005L
    #define EXCEPTION_ARRAY_BOUNDS_EXCEEDED 0xC000008CL
    #define EXCEPTION_BREAKPOINT 0x80000003L
    #define EXCEPTION_DATATYPE_MISALIGNMENT 0x80000002L
    #define EXCEPTION_FLT_DENORMAL_OPERAND 0xC000008DL
    #define EXCEPTION_FLT_DIVIDE_BY_ZERO 0xC000008EL
    #define EXCEPTION_FLT_INEXACT_RESULT 0xC000008FL
    #define EXCEPTION_FLT_INVALID_OPERATION 0xC0000090L
    #define EXCEPTION_FLT_OVERFLOW 0xC0000091L
    #define EXCEPTION_FLT_STACK_CHECK 0xC0000092L
    #define EXCEPTION_FLT_UNDERFLOW 0xC0000093L
    #define EXCEPTION_ILLEGAL_INSTRUCTION 0xC000001DL
    #define EXCEPTION_INT_DIVIDE_BY_ZERO 0xC0000094L
    #define EXCEPTION_INT_OVERFLOW 0xC0000095L
    #define EXCEPTION_PRIV_INSTRUCTION 0xC0000096L
    #define EXCEPTION_STACK_OVERFLOW 0xC00000FDL
    #define EXCEPTION_IN_PAGE_ERROR 0xC0000006L
    #define EXCEPTION_INVALID_DISPOSITION 0xC0000026L
    #define EXCEPTION_NONCONTINUABLE_EXCEPTION 0xC0000025L
    #define EXCEPTION_CONTINUE_SEARCH 0L
    #define EXCEPTION_EXECUTE_HANDLER 1

    // VK_* macros → canonical Vk::* constants
    #define VK_LBUTTON   Vk::LBUTTON
    #define VK_RBUTTON   Vk::RBUTTON
    #define VK_MBUTTON   Vk::MBUTTON
    #define VK_XBUTTON1  Vk::XBUTTON1
    #define VK_XBUTTON2  Vk::XBUTTON2
    #define VK_BACK      Vk::BACK
    #define VK_TAB       Vk::TAB
    #define VK_CLEAR     Vk::CLEAR
    #define VK_RETURN    Vk::RETURN
    #define VK_SHIFT     Vk::SHIFT
    #define VK_CONTROL   Vk::CONTROL
    #define VK_MENU      Vk::MENU
    #define VK_PAUSE     Vk::PAUSE
    #define VK_CAPITAL   Vk::CAPITAL
    #define VK_ESCAPE    Vk::ESCAPE
    #define VK_SPACE     Vk::SPACE
    #define VK_PRIOR     Vk::PRIOR
    #define VK_NEXT      Vk::NEXT
    #define VK_END       Vk::END
    #define VK_HOME      Vk::HOME
    #define VK_LEFT      Vk::LEFT
    #define VK_UP        Vk::UP
    #define VK_RIGHT     Vk::RIGHT
    #define VK_DOWN      Vk::DOWN
    #define VK_SNAPSHOT  Vk::SNAPSHOT
    #define VK_INSERT    Vk::INSERT
    #define VK_DELETE    Vk::DELETE
    #define VK_LWIN      Vk::LWIN
    #define VK_RWIN      Vk::RWIN
    #define VK_APPS      Vk::APPS
    #define VK_NUMLOCK   Vk::NUMLOCK
    #define VK_SCROLL    Vk::SCROLL
    #define VK_LSHIFT    Vk::LSHIFT
    #define VK_RSHIFT    Vk::RSHIFT
    #define VK_LCONTROL  Vk::LCONTROL
    #define VK_RCONTROL  Vk::RCONTROL
    #define VK_LMENU     Vk::LMENU
    #define VK_RMENU     Vk::RMENU
    #define VK_OEM_1     Vk::OEM_1
    #define VK_OEM_PLUS  Vk::OEM_PLUS
    #define VK_OEM_COMMA Vk::OEM_COMMA
    #define VK_OEM_MINUS Vk::OEM_MINUS
    #define VK_OEM_PERIOD Vk::OEM_PERIOD
    #define VK_OEM_2     Vk::OEM_2
    #define VK_OEM_3     Vk::OEM_3
    #define VK_OEM_4     Vk::OEM_4
    #define VK_OEM_5     Vk::OEM_5
    #define VK_OEM_6     Vk::OEM_6
    #define VK_OEM_7     Vk::OEM_7
    #define VK_OEM_102   Vk::OEM_102
    #define VK_SEPARATOR Vk::SEPARATOR
    #define VK_ADD       Vk::ADD
    #define VK_SUBTRACT  Vk::SUBTRACT
    #define VK_MULTIPLY  Vk::MULTIPLY
    #define VK_DIVIDE    Vk::DIVIDE
    #define VK_DECIMAL   Vk::DECIMAL
    #define VK_F1        Vk::F1
    #define VK_F2        Vk::F2
    #define VK_F3        Vk::F3
    #define VK_F4        Vk::F4
    #define VK_F5        Vk::F5
    #define VK_F6        Vk::F6
    #define VK_F7        Vk::F7
    #define VK_F8        Vk::F8
    #define VK_F9        Vk::F9
    #define VK_F10       Vk::F10
    #define VK_F11       Vk::F11
    #define VK_F12       Vk::F12
    #define VK_F13       Vk::F13
    #define VK_F14       Vk::F14
    #define VK_F15       Vk::F15
    #define VK_F16       Vk::F16
    #define VK_F17       Vk::F17
    #define VK_F18       Vk::F18
    #define VK_F19       Vk::F19
    #define VK_F20       Vk::F20
    #define VK_F21       Vk::F21
    #define VK_F22       Vk::F22
    #define VK_F23       Vk::F23
    #define VK_F24       Vk::F24
    #define VK_NUMPAD0   Vk::NUMPAD0
    #define VK_NUMPAD1   Vk::NUMPAD1
    #define VK_NUMPAD2   Vk::NUMPAD2
    #define VK_NUMPAD3   Vk::NUMPAD3
    #define VK_NUMPAD4   Vk::NUMPAD4
    #define VK_NUMPAD5   Vk::NUMPAD5
    #define VK_NUMPAD6   Vk::NUMPAD6
    #define VK_NUMPAD7   Vk::NUMPAD7
    #define VK_NUMPAD8   Vk::NUMPAD8
    #define VK_NUMPAD9   Vk::NUMPAD9

    // MapVirtualKey constants (used in gui_input.cpp)
    #define MAPVK_VK_TO_VSC    0
    #define MAPVK_VSC_TO_VK    1
    #define MAPVK_VK_TO_CHAR   2
    #define MAPVK_VSC_TO_VK_EX 3
    #define MAPVK_VK_TO_VSC_EX 4

    // Additional Windows compatibility macros
    #define MAX_PATH 4096
    #define CP_UTF8 65001
    #define WM_APP 0x8000
    #define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 4
    #define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 2
    #define GENERIC_WRITE 0x40000000
    #define FILE_ATTRIBUTE_NORMAL 0x80
    #define CREATE_ALWAYS 2
    #define CREATE_NEW 1
    #define ERROR_FILE_EXISTS 80
    #define INVALID_HANDLE_VALUE reinterpret_cast<void*>(-1)
    #define RT_RCDATA MAKEINTRESOURCEW(10)

    // Win32 string type aliases
    using LPCWSTR = const wchar_t*;
    using LPCTSTR = const wchar_t*;
    using LPTSTR = wchar_t*;
    using LPWSTR = wchar_t*;
    using PWSTR = wchar_t*;
    using LPSTR = char*;
    using LPCSTR = const char*;
    using UINT_PTR = uintptr_t;
    using HHOOK = void*;

    // Memory/hook stubs
    struct MEMORY_BASIC_INFORMATION { void* BaseAddress; void* AllocationBase; unsigned long AllocationProtect; size_t RegionSize; unsigned long State; unsigned long Protect; unsigned long Type; };
    inline size_t VirtualQuery(const void*, MEMORY_BASIC_INFORMATION*, size_t) { return 0; }
    inline int FreeLibrary(void*) { return 0; }
    inline int GetModuleHandleExA(unsigned long, LPCSTR, HMODULE*) { return 0; }

    // Additional Win32 types
    union LARGE_INTEGER { int64_t QuadPart; int32_t LowPart; int32_t HighPart; struct { int32_t LowPart; int32_t HighPart; } u; };
    #define IMAGE_CURSOR 2
    #define IMAGE_ICON 1
    #define IMAGE_BITMAP 0
    #define LR_DEFAULTSIZE 0x0040
    #define LR_COPYFROMRESOURCE 0x4000
    #define ERROR_ACCESS_DENIED 5L
    #define ERROR_LOCK_VIOLATION 33L
    #define ERROR_FILE_NOT_FOUND 2L
    #define FOLDERID_Profile {0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}  // dummy GUID

    // QueryPerformanceCounter polyfill
    inline int QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        lpPerformanceCount->QuadPart = static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
        return 1;
    }
    inline int QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency) {
        lpFrequency->QuadPart = 1000000000LL;  // 1 GHz (nanoseconds)
        return 1;
    }

    // GetCommandLineW polyfill
    inline LPWSTR GetCommandLineW() {
        static wchar_t buf[4096];
        std::ifstream cmdline("/proc/self/cmdline");
        if (cmdline) {
            std::string s;
            char c;
            while (cmdline.get(c)) s += (c == '\0' ? ' ' : c);
            if (!s.empty() && s.back() == ' ') s.pop_back();
            mbstowcs(buf, s.c_str(), 4095);
        } else {
            buf[0] = L'\0';
        }
        return buf;
    }

    // MAKEINTRESOURCEW stub (not functional on Linux — resources are files)
    inline LPWSTR MAKEINTRESOURCEW(int i) { return reinterpret_cast<LPWSTR>(static_cast<uintptr_t>(i)); }

    // GetTickCount64 polyfill
    inline uint64_t GetTickCount64() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1000000ULL;
    }

    // _wgetenv_s polyfill
    inline int _wgetenv_s(size_t* requiredSize, wchar_t* buffer, size_t bufferSize, const wchar_t* name) {
        char nameBuf[256];
        wcstombs(nameBuf, name, sizeof(nameBuf) - 1);
        const char* val = ::getenv(nameBuf);
        if (!val) { if (requiredSize) *requiredSize = 0; return 1; }
        size_t len = strlen(val);
        if (requiredSize) *requiredSize = len;
        if (buffer && bufferSize > len) { mbstowcs(buffer, val, bufferSize); return 0; }
        return 1;
    }

    // DeleteFileW polyfill
    inline int DeleteFileW(const wchar_t* path) {
        char pathBuf[4096];
        wcstombs(pathBuf, path, sizeof(pathBuf) - 1);
        return ::unlink(pathBuf) == 0 ? 1 : 0;
    }

    // Win32 file API stubs
    inline int GetFileAttributesW(const wchar_t*) { return -1; }  // INVALID_FILE_ATTRIBUTES
    #define INVALID_FILE_ATTRIBUTES ((int)-1)
    #define FILE_ATTRIBUTE_DIRECTORY 0x10

    // Additional stubs
    inline int DestroyIcon(void*) { return 1; }
    // FormatMessage stubs
    #define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x100
    #define FORMAT_MESSAGE_FROM_SYSTEM 0x1000
    #define FORMAT_MESSAGE_IGNORE_INSERTS 0x200
    #define LANG_NEUTRAL 0
    #define SUBLANG_DEFAULT 1
    #define MAKELANGID(a, b) ((static_cast<unsigned short>(a)) | (static_cast<unsigned short>(b) << 10))
    inline unsigned long FormatMessageA(unsigned long, const void*, unsigned long, unsigned long, char* buf, unsigned long, void*) {
        const char* msg = strerror(errno);
        if (buf) { strncpy(buf, msg, 255); buf[255] = '\0'; }
        return static_cast<unsigned long>(strlen(buf));
    }
    using HLOCAL = void*;
    inline HLOCAL LocalFree(void*) { return nullptr; }

    // CaptureStackBackTrace polyfill
    #include <execinfo.h>
    inline unsigned short CaptureStackBackTrace(unsigned long, unsigned long, void** frames, unsigned long*) {
        return static_cast<unsigned short>(::backtrace(frames, 32));
    }

    // localtime_s polyfill
    inline int localtime_s(struct tm* result, const time_t* timer) {
        struct tm* t = localtime_r(timer, result);
        return t ? 0 : 1;
    }

    // Window stubs (no-op on Linux)
    inline int IsWindow(void*) { return 1; }
    inline void* GetForegroundWindow() { return nullptr; }

    inline void TerminateProcess(void*, unsigned int) { ::_exit(1); }
    inline unsigned long GetLastError() { return static_cast<unsigned long>(errno); }
    inline unsigned long GetCurrentThreadId() { return static_cast<unsigned long>(syscall(SYS_gettid)); }
    inline int GetSystemMetrics(int) { return 1920; }
    #define SM_CXSCREEN 0
    #define SM_CYSCREEN 1

    // More Win32 stubs
    #define GA_ROOTOWNER 3
    inline void* GetAncestor(void*, int) { return nullptr; }
    #define MOVEFILE_REPLACE_EXISTING 1
    #define MOVEFILE_WRITE_THROUGH 8
    inline int MoveFileExW(const wchar_t*, const wchar_t*, unsigned long) { return 0; }
    struct FILETIME { unsigned long dwLowDateTime; unsigned long dwHighDateTime; };
    union ULARGE_INTEGER { uint64_t QuadPart; unsigned long LowPart; unsigned long HighPart; struct { unsigned long LowPart; unsigned long HighPart; } u; };
    inline int FileTimeToSystemTime(const FILETIME*, void*) { return 0; }
    inline int LocalFileTimeToFileTime(const FILETIME*, FILETIME*) { return 0; }
    inline int FileTimeToLocalFileTime(const FILETIME*, FILETIME*) { return 0; }
    inline int CompareFileTime(const FILETIME*, const FILETIME*) { return 0; }

    // Window manipulation stubs
    inline int GetWindowThreadProcessId(void*, DWORD*) { return 0; }
    inline intptr_t GetWindowLongPtr(void*, int) { return 0; }
    inline intptr_t SetWindowLongPtr(void*, int, intptr_t) { return 0; }
    #define GWL_STYLE (-16)
    #define GWL_EXSTYLE (-20)
    #define WS_POPUP 0x80000000L
    #define WS_VISIBLE 0x10000000L
    #define WS_EX_TOPMOST 8
    #define HWND_TOPMOST reinterpret_cast<void*>(-1)
    inline int SetWindowPos(void*, void*, int, int, int, int, unsigned int) { return 0; }
    #define SWP_NOMOVE 2
    #define SWP_NOSIZE 1
    #define SWP_NOZORDER 4
    #define SWP_FRAMECHANGED 0x20
    #define HWND_NOTOPMOST reinterpret_cast<void*>(-2)

    // Clipboard stub
    inline void* GlobalLock(void*) { return nullptr; }
    inline int GlobalUnlock(void*) { return 0; }
    inline int OpenClipboard(void*) { return 0; }
    inline int CloseClipboard() { return 0; }
    inline int EmptyClipboard() { return 0; }
    inline void* SetClipboardData(unsigned int, void*) { return nullptr; }
    #define CF_TEXT 1
    #define CF_UNICODETEXT 13
    #define CF_BITMAP 2

    // Process info stubs
    #define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
    inline void* OpenProcess(unsigned long, int, unsigned long) { return nullptr; }
    inline int CloseHandle(void*) { return 1; }
    inline int GetProcessTimes(void*, FILETIME*, FILETIME*, FILETIME*, FILETIME*) { return 0; }

    // File attribute stubs
    struct WIN32_FILE_ATTRIBUTE_DATA {
        unsigned long dwFileAttributes;
        FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
        unsigned long nFileSizeHigh, nFileSizeLow;
    };
    #define GetFileExInfoStandard 0
    inline int GetFileAttributesExW(const wchar_t*, int, WIN32_FILE_ATTRIBUTE_DATA*) { return 0; }

    // Time stubs
    struct SYSTEMTIME { unsigned short wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; };
    inline void GetSystemTimeAsFileTime(FILETIME* ft) {
        auto now = std::chrono::system_clock::now();
        auto sinceEpoch = now.time_since_epoch();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(sinceEpoch).count();
        uint64_t ftVal = static_cast<uint64_t>(ns) / 100 + 116444736000000000ULL;
        ft->dwLowDateTime  = static_cast<unsigned long>(ftVal & 0xFFFFFFFF);
        ft->dwHighDateTime = static_cast<unsigned long>(ftVal >> 32);
    }
    inline int SystemTimeToFileTime(const SYSTEMTIME*, FILETIME* ft) { GetSystemTimeAsFileTime(ft); return 1; }
    inline int FileTimeToSystemTime(const FILETIME*, SYSTEMTIME* st) {
        memset(st, 0, sizeof(*st)); return 1;
    }

    // Shell stubs
    #define CSIDL_LOCAL_APPDATA 0x001c
    inline int SHGetFolderPathW(void*, int, void*, unsigned long, wchar_t* path) {
        const char* xdg = getenv("XDG_DATA_HOME");
        if (!xdg) xdg = getenv("HOME");
        if (xdg) { mbstowcs(path, xdg, 255); return 0; }
        wcscpy(path, L"/tmp"); return 0;
    }

    // Additional stubs
    inline int GetTempPathW(unsigned long, wchar_t* path) { wcscpy(path, L"/tmp/"); return 4; }
    inline void GetLocalTime(SYSTEMTIME* st) {
        auto now = std::chrono::system_clock::now();
        time_t t = std::chrono::system_clock::to_time_t(now);
        struct tm* tm = localtime(&t);
        st->wYear = tm->tm_year + 1900; st->wMonth = tm->tm_mon + 1; st->wDay = tm->tm_mday;
        st->wHour = tm->tm_hour; st->wMinute = tm->tm_min; st->wSecond = tm->tm_sec; st->wMilliseconds = 0;
    }
    inline int WriteFile(void*, const void*, unsigned long, DWORD*, void*) { return 0; }
    inline int FlushFileBuffers(void*) { return 0; }
    inline int MoveFileW(const wchar_t*, const wchar_t*) { return 0; }

    // Shell/filesystem stubs
    #define CSIDL_PROFILE 0x0028
    #define SUCCEEDED(hr) (static_cast<int>(hr) >= 0)
    using WCHAR = wchar_t;
    inline int GetModuleHandleEx(unsigned long, const wchar_t*, void**) { return 0; }
    inline int GetCurrentDirectoryW(unsigned long, wchar_t* buf) { if (buf) wcscpy(buf, L"."); return 1; }
    inline int SHCreateDirectoryExW(void*, const wchar_t*, void*) { return 0; }
    inline LONG (*SetUnhandledExceptionFilter(LONG (*)(EXCEPTION_POINTERS*)))(EXCEPTION_POINTERS*) { return nullptr; }
    inline void _set_se_translator(void (*)(unsigned int, EXCEPTION_POINTERS*)) {}

    // Error codes
    #define ERROR_SUCCESS 0L
    #define ERROR_INSUFFICIENT_BUFFER 122L

    // File search stubs
    struct WIN32_FIND_DATAW { unsigned long dwFileAttributes; FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime; unsigned long nFileSizeHigh, nFileSizeLow; wchar_t cFileName[260]; wchar_t cAlternateFileName[14]; };
    inline void* FindFirstFileW(const wchar_t*, WIN32_FIND_DATAW*) { return reinterpret_cast<void*>(-1); }
    inline int FindNextFileW(void*, WIN32_FIND_DATAW*) { return 0; }
    inline int FindClose(void*) { return 0; }
    #define INVALID_HANDLE_VALUE_FILE reinterpret_cast<void*>(-1)

    // Window visibility stubs
    inline int IsWindowVisible(void*) { return 0; }
    inline int GetWindowRect(void*, RECT* r) { if (r) { r->left = r->top = 0; r->right = 1920; r->bottom = 1080; } return 1; }
    using HMONITOR = void*;
    inline HMONITOR MonitorFromWindow(void*, unsigned long) { return nullptr; }
    #define MONITOR_DEFAULTTONEAREST 2
    inline HMONITOR MonitorFromRect(const RECT*, unsigned long) { return nullptr; }
    struct MONITORINFO { unsigned long cbSize; RECT rcMonitor; RECT rcWork; unsigned long dwFlags; };
    inline int GetMonitorInfo(HMONITOR, MONITORINFO* mi) { if (mi) { mi->rcMonitor = {0,0,1920,1080}; mi->rcWork = {0,0,1920,1080}; } return 1; }
    struct POINT { int32_t x, y; };
    inline int GetClientRect(void*, RECT* r) { if (r) { r->left = r->top = 0; r->right = 1920; r->bottom = 1080; } return 1; }
    inline int ScreenToClient(void*, POINT*) { return 1; }
    using LPPOINT = POINT*;
    inline int ClientToScreen(void*, POINT*) { return 1; }

    // Cursor stubs
    struct CURSORINFO { unsigned long cbSize; unsigned long flags; void* hCursor; POINT ptScreenPos; };
    #define CURSOR_SHOWING 1
    inline int GetCursorInfo(CURSORINFO* ci) { if (ci) { ci->flags = 0; ci->hCursor = nullptr; ci->ptScreenPos = {0,0}; } return 1; }
    inline void* GetCursor() { return nullptr; }

    // Path/shell stubs
    inline int PathIsRelativeW(const wchar_t*) { return 1; }
    inline unsigned int GetDpiForWindow(void*) { return 96; }
    #define USER_DEFAULT_SCREEN_DPI 96
    inline int AdjustWindowRectExForDpi(RECT*, unsigned long, int, unsigned long, unsigned int) { return 1; }

    // File stubs
    #define GENERIC_READ 0x80000000L
    inline int ReadFile(void*, void*, unsigned long, DWORD*, void*) { return 0; }
    inline void Sleep(unsigned long ms) { usleep(ms * 1000); }

    // More file stubs
    inline int fopen_s(FILE** f, const char* path, const char* mode) { *f = fopen(path, mode); return *f ? 0 : 1; }
    inline int GetFileTime(void*, FILETIME*, FILETIME*, FILETIME*) { return 1; }
    inline unsigned long SetFilePointer(void*, long, long*, unsigned long) { return 0; }
    #define FILE_BEGIN 0
    #define INVALID_SET_FILE_POINTER 0xFFFFFFFF
    inline unsigned long GetFileSize(void*, unsigned long*) { return 0; }

    // Input stubs
    inline short GetKeyState(int) { return 0; }
    inline short GetAsyncKeyState(int) { return 0; }
    using BYTE = uint8_t;
    using SIZE_T = size_t;

    // Bitmap/clipboard stubs
    #define BI_RGB 0
    #define CF_DIB 8
    #define GMEM_MOVEABLE 2
    struct BITMAPINFOHEADER { unsigned long biSize; long biWidth, biHeight; unsigned short biPlanes, biBitCount; unsigned long biCompression, biSizeImage; long biXPelsPerMeter, biYPelsPerMeter; unsigned long biClrUsed, biClrImportant; };
    inline void* GlobalAlloc(unsigned int, size_t) { return malloc(1024); }
    inline void* GlobalFree(void* p) { free(p); return nullptr; }

    // Directory stubs
    inline int CreateDirectoryW(const wchar_t*, void*) { return 1; }
    inline int CopyFileW(const wchar_t*, const wchar_t*, int) { return 0; }

    // Window message stubs
    inline unsigned int RegisterWindowMessageA(const char*) { return 0; }
    inline int IsIconic(void*) { return 0; }
    inline int IsZoomed(void*) { return 0; }
    #define SW_RESTORE 9
    inline int ShowWindow(void*, int) { return 1; }
    #define SWP_NOOWNERZORDER 0x200
    #define WM_SIZE 5
    #define SIZE_RESTORED 0
    #define MAKELPARAM(a,b) static_cast<LPARAM>((static_cast<unsigned short>(a)) | (static_cast<unsigned int>(b) << 16))
    inline int PostMessage(void*, unsigned int, WPARAM, LPARAM) { return 1; }
    inline void SetLastError(unsigned long) {}
    using LONG_PTR = intptr_t;
    #define WS_CAPTION 0xC00000
    #define WS_BORDER 0x800000
    #define WS_DLGFRAME 0x400000
    #define WS_THICKFRAME 0x40000
    #define WS_MINIMIZEBOX 0x20000
    #define WS_MAXIMIZEBOX 0x10000
    #define WS_SYSMENU 0x80000
    #define WS_MINIMIZE 0x20000000
    #define WS_MAXIMIZE 0x1000000
    #define WS_CHILD 0x40000000
    #define WS_OVERLAPPED 0
    #define WS_OVERLAPPEDWINDOW (WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX)
    #define WS_EX_TOOLWINDOW 0x80
    #define WS_EX_WINDOWEDGE 0x100
    #define WS_EX_CLIENTEDGE 0x200
    #define WS_EX_DLGMODALFRAME 1
    #define WS_EX_STATICEDGE 0x20000
    #define WS_EX_APPWINDOW 0x40000
    #define WS_SIZEBOX WS_THICKFRAME
    #define SW_SHOW 5
    #define SW_HIDE 0

    // Registry stubs
    using HKEY = void*;
    #define HKEY_CLASSES_ROOT reinterpret_cast<void*>(0x80000000)
    #define KEY_READ 0x20019
    inline int RegOpenKeyExA(void*, const char*, unsigned long, unsigned long, HKEY*) { return 1; }
    inline int RegCloseKey(void*) { return 0; }

    // Cursor stubs
    #define IDC_ARROW reinterpret_cast<wchar_t*>(32512)
    #define IS_INTRESOURCE(x) (reinterpret_cast<uintptr_t>(x) >> 16 == 0)
    #define LR_LOADFROMFILE 16
    typedef void* HANDLE;
    inline void* LoadImageW(void*, const wchar_t*, unsigned int, int, int, unsigned int) { return nullptr; }
    inline int DestroyCursor(void*) { return 1; }
    #define ERROR_PATH_NOT_FOUND 3L

    // Memory/file mapping stubs
    inline void MemoryBarrier() { __sync_synchronize(); }
    inline int GetFileAttributesA(const char*) { return -1; }
    #define FILE_MAP_READ 4
    inline void* OpenFileMappingW(unsigned long, int, const wchar_t*) { return nullptr; }
    inline void* MapViewOfFile(void*, unsigned long, unsigned long, unsigned long, size_t) { return nullptr; }
    inline int GetFileSizeEx(void*, LARGE_INTEGER*) { return 0; }
    inline int UnmapViewOfFile(void*) { return 1; }
    #define PAGE_READWRITE 4
    #define PAGE_READONLY 2
    #define PAGE_EXECUTE 0x10
    #define PAGE_EXECUTE_READ 0x20
    #define PAGE_EXECUTE_READWRITE 0x40
    #define PAGE_EXECUTE_WRITECOPY 0x80
    #define PAGE_WRITECOPY 8
    #define MEM_COMMIT 0x1000
    #define MEM_RESERVE 0x2000
    #define WM_ACTIVATEAPP 0x1C
    #define WM_MOVE 3
    #define WM_MOVING 0x216
    #define SIZE_MINIMIZED 1
    inline int ShowCursor(int) { return 0; }
    inline LRESULT CallWindowProc(WNDPROC, HWND, UINT, WPARAM, LPARAM) { return 0; }
    inline DWORD GetFileVersionInfoSizeW(const wchar_t*, DWORD*) { return 0; }
    inline int GetFileVersionInfoW(const wchar_t*, unsigned long, unsigned long, void*) { return 0; }
    inline int VerQueryValueW(const void*, const wchar_t*, void**, unsigned int*) { return 0; }
    inline unsigned long GetModuleFileNameW(HMODULE, wchar_t*, unsigned long) { return 0; }

    // Module info stubs
    struct MODULEINFO { void* lpBaseOfDll; unsigned long SizeOfImage; void* EntryPoint; };
    inline int GetModuleInformation(void*, HMODULE, MODULEINFO*, unsigned long) { return 0; }

    // PE parsing stubs
    #define IMAGE_DOS_SIGNATURE 0x5A4D
    #define IMAGE_NT_SIGNATURE 0x4550
    #define IMAGE_DIRECTORY_ENTRY_IMPORT 1
    struct IMAGE_DOS_HEADER { unsigned short e_magic; unsigned char _pad[58]; long e_lfanew; };
    struct IMAGE_FILE_HEADER { unsigned short Machine; unsigned short NumberOfSections; unsigned long TimeDateStamp; unsigned long PointerToSymbolTable; unsigned long NumberOfSymbols; unsigned short SizeOfOptionalHeader; unsigned short Characteristics; };
    struct IMAGE_DATA_DIRECTORY { unsigned long VirtualAddress; unsigned long Size; };
    struct IMAGE_OPTIONAL_HEADER { unsigned short Magic; unsigned char _pad1[94]; IMAGE_DATA_DIRECTORY DataDirectory[16]; };
    struct IMAGE_NT_HEADERS { unsigned long Signature; IMAGE_FILE_HEADER FileHeader; IMAGE_OPTIONAL_HEADER OptionalHeader; };
    using PIMAGE_DOS_HEADER = IMAGE_DOS_HEADER*;
    using PIMAGE_NT_HEADERS = IMAGE_NT_HEADERS*;
    struct IMAGE_IMPORT_DESCRIPTOR { unsigned long OriginalFirstThunk; unsigned long TimeDateStamp; unsigned long ForwarderChain; unsigned long Name; unsigned long FirstThunk; };
    using PIMAGE_IMPORT_DESCRIPTOR = IMAGE_IMPORT_DESCRIPTOR*;
    struct IMAGE_THUNK_DATA { union { unsigned long Function; unsigned long Ordinal; unsigned long AddressOfData; } u1; };
    using PIMAGE_THUNK_DATA = IMAGE_THUNK_DATA*;
    struct IMAGE_IMPORT_BY_NAME { unsigned short Hint; char Name[1]; };
    using PIMAGE_IMPORT_BY_NAME = IMAGE_IMPORT_BY_NAME*;
    #define IMAGE_SNAP_BY_ORDINAL(Ordinal) ((Ordinal) & 0x80000000)
    #define IMAGE_ORDINAL_FLAG 0x80000000
    inline HMODULE GetModuleHandle(const wchar_t*) { return nullptr; }
    inline void* GetProcAddress(HMODULE, const char*) { return nullptr; }
    #define WM_SIZING 0x214
    #define WM_WINDOWPOSCHANGED 0x47
    #define WM_DPICHANGED 0x2E0
    #define WM_DISPLAYCHANGE 0x7E
    #define WM_MOUSEFIRST 0x200
    #define WM_MOUSELAST 0x20D
    #define WM_SYSCHAR 0x106
    #define WM_DEADCHAR 0x103
    #define WM_SYSDEADCHAR 0x107
    #define WM_TIMER 0x113
    #define INFINITE 0xFFFFFFFF
    #define WAIT_OBJECT_0 0
    inline unsigned long WaitForMultipleObjects(unsigned long, void* const*, int, unsigned long) { return WAIT_OBJECT_0; }
    inline int CancelWaitableTimer(void*) { return 1; }
    inline int SetWaitableTimer(void*, const LARGE_INTEGER*, long, void*, void*, int) { return 1; }
    inline int PostMessageW(void*, unsigned int, WPARAM, LPARAM) { return 1; }
    inline void* CreateEventW(void*, int, int, const wchar_t*) { return reinterpret_cast<void*>(1); }
    #define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 2
    #define TIMER_ALL_ACCESS 0x1F0003
    inline void* CreateWaitableTimerExW(void*, const wchar_t*, unsigned long, unsigned long) { return reinterpret_cast<void*>(1); }
    inline void* CreateWaitableTimerW(void*, const wchar_t*, int, const wchar_t*) { return reinterpret_cast<void*>(1); }
    inline void* CreateWaitableTimerW(const wchar_t*, int, const wchar_t*) { return reinterpret_cast<void*>(1); }
    inline HANDLE CreateThread(void*, size_t, void* (*)(void*), void*, unsigned long, unsigned long*) { return nullptr; }
    inline uintptr_t SetTimer(void*, uintptr_t, unsigned int, void*) { return 1; }
    using HKL = void*;
    inline unsigned long GetMessageTime() { return 0; }
    inline short VkKeyScanExW(wchar_t, HKL) { return -1; }
    #define HIBYTE(w) static_cast<unsigned char>(((w) >> 8) & 0xFF)
    #define LOBYTE(w) static_cast<unsigned char>((w) & 0xFF)
    inline int GetKeyboardState(unsigned char*) { return 0; }
    struct KBDLLHOOKSTRUCT { unsigned long vkCode; unsigned long scanCode; unsigned long flags; unsigned long time; ULONG_PTR dwExtraInfo; };
    #define LLKHF_EXTENDED 1
    using HHOOK = void*;
    using HKL = void*;  // Forward declaration for VkKeyScanExW
    inline HHOOK SetWindowsHookEx(int, void*, HINSTANCE, unsigned long) { return nullptr; }
    inline int UnhookWindowsHookEx(HHOOK) { return 0; }
    inline LRESULT CallNextHookEx(HHOOK, int, WPARAM, LPARAM) { return 0; }
    #define WH_KEYBOARD_LL 13
    #define WH_MOUSE_LL 14
    inline int KillTimer(void*, uintptr_t) { return 0; }
    inline int SetEvent(void*) { return 1; }
    inline HMODULE GetModuleHandleA(const char*) { return nullptr; }
    struct WINDOWPOS { HWND hwnd; HWND hwndInsertAfter; int x, y, cx, cy; unsigned int flags; };
    #define WM_INPUT 0xFF
    #define WM_DESTROY 2
    inline LRESULT DefWindowProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
    inline int EnumProcessModules(void*, HMODULE*, unsigned long, DWORD*) { return 0; }

    // MinHook stubs (compilation only — MinHook is Windows-only)
    using MH_STATUS = int;
    #define MH_OK 0
    #define MH_ERROR_ALREADY_CREATED 1
    #define MH_ERROR_ENABLED 2
    inline MH_STATUS MH_EnableHook(void*) { return MH_OK; }
    inline MH_STATUS MH_RemoveHook(void*) { return MH_OK; }
    inline MH_STATUS MH_CreateHook(void*, void*, void**) { return MH_OK; }
    #define OPEN_ALWAYS 4

    // XBUTTON1/2 as constexpr (not macros — avoids conflict with Vk::XBUTTON1)
    constexpr unsigned short XBUTTON1 = 1;
    constexpr unsigned short XBUTTON2 = 2;
    inline void* CreateFileMappingW(void*, void*, unsigned long, unsigned long, unsigned long, const wchar_t*) { return nullptr; }
    #define FILE_MAP_ALL_ACCESS 0xF001F
    inline int GetCursorPos(POINT* p) { if (p) { p->x = p->y = 0; } return 1; }

    // More error codes
    #define ERROR_INVALID_PARAMETER 87L
    #define ERROR_NOT_ENOUGH_MEMORY 8L
    #define ERROR_RESOURCE_TYPE_NOT_FOUND 1813L

    // Image/cursor stubs
    inline void* CopyImage(void*, unsigned int, int, int, unsigned int) { return nullptr; }
    struct ICONINFOEXW { unsigned long cbSize; int fIcon; int xHotspot, yHotspot; void* hbmMask; void* hbmColor; unsigned short wResID; wchar_t szModName[260]; wchar_t szResName[260]; };
    inline int GetIconInfoExW(void*, ICONINFOEXW*) { return 0; }
    struct BITMAP { long bmType, bmWidth, bmHeight, bmWidthBytes; unsigned short bmPlanes, bmBitsPixel; void* bmBits; };
    inline int GetObject(void*, int, void*) { return 0; }
    inline int DeleteObject(void*) { return 0; }
    inline void* CreateCompatibleDC(void*) { return nullptr; }
    inline int DeleteDC(void*) { return 0; }
    inline void* GetDC(void*) { return nullptr; }
    inline int ReleaseDC(void*, void*) { return 1; }
    inline int GetWindowTextA(void*, char*, int) { return 0; }
    inline int GetClassNameA(void*, char*, int) { return 0; }
    inline int EnumWindows(int (*)(void*, LPARAM), LPARAM) { return 0; }
    inline int InvalidateRect(void*, const RECT*, int) { return 1; }
    #define RDW_INVALIDATE 1
    #define RDW_FRAME 2
    #define RDW_ALLCHILDREN 0x80
    struct BITMAPINFO { BITMAPINFOHEADER bmiHeader; };
    #define DIB_RGB_COLORS 0
    inline int GetDIBits(void*, void*, unsigned int, unsigned int, void*, BITMAPINFO*, unsigned int) { return 0; }
    inline void* SelectObject(void*, void*) { return nullptr; }
    inline void* CreateDIBSection(void*, BITMAPINFO*, unsigned int, void**, void*, unsigned long) { return nullptr; }
    inline void* wglGetCurrentContext() { return nullptr; }
    inline void* LoadCursorW(void*, const wchar_t*) { return nullptr; }
    inline int RedrawWindow(void*, const RECT*, void*, unsigned int) { return 1; }
    #define WM_PAINT 15
    #define WM_CAPTURECHANGED 0x215
    #define SMTO_ABORTIFHUNG 2
    #define SMTO_NORMAL 0
    inline int SendMessageTimeoutW(void*, unsigned int, WPARAM, LPARAM, unsigned int, unsigned int, DWORD*) { return 0; }
    inline void* CreateCompatibleBitmap(void*, int, int) { return nullptr; }
    #define DKGRAY_BRUSH 3
    inline void* GetStockObject(int) { return nullptr; }
    inline int FillRect(void*, const RECT*, void*) { return 1; }
    #define R2_COPYPEN 13
    inline int SetROP2(void*, int) { return 0; }
    using HRESULT = long;
    #define SRCCOPY 0xCC0020
    inline int BitBlt(void*, int, int, int, int, void*, int, int, unsigned long) { return 1; }

    // Fix ReadFile 2nd overload for DWORD*
    inline int ReadFile(void*, void*, unsigned long, unsigned long*, void*) { return 0; }

    // DWM stubs
    inline int DwmGetWindowAttribute(void*, unsigned long, void*, unsigned long) { return 0; }
    #define PW_RENDERFULLCONTENT 2
    inline int PrintWindow(void*, void*, unsigned int) { return 0; }

    // More message constants
    #define WM_SETFOCUS 7
    #define WM_KILLFOCUS 8
    #define WM_ACTIVATE 6
    #define WA_ACTIVE 1
    #define WA_INACTIVE 0
    #define WM_MOUSEWHEEL 0x20A
    #define WM_MOUSEHWHEEL 0x20E
    #define WM_KEYDOWN 0x100
    #define WM_KEYUP 0x101
    #define WM_CHAR 0x102
    #define WM_SYSKEYDOWN 0x104
    #define WM_SYSKEYUP 0x105
    #define WHEEL_DELTA 120
    #define HIWORD(l) ((unsigned short)(((l) >> 16) & 0xFFFF))
    #define KF_EXTENDED 0x100
    #define WM_LBUTTONDOWN 0x201
    #define WM_LBUTTONUP 0x202
    #define WM_RBUTTONDOWN 0x204
    #define WM_RBUTTONUP 0x205
    #define WM_MBUTTONDOWN 0x207
    #define WM_MBUTTONUP 0x208
    #define WM_XBUTTONDOWN 0x20B
    #define WM_XBUTTONUP 0x20C
    #define WM_XBUTTONDBLCLK 0x20D
    #define WM_LBUTTONDBLCLK 0x203
    #define WM_RBUTTONDBLCLK 0x206
    #define WM_MBUTTONDBLCLK 0x209
    #define WM_MOUSEMOVE 0x200
    #define WM_MOUSELEAVE 0x2A3
    #define WM_NCMOUSEMOVE 0xA0
    inline int SendMessage(void*, unsigned int, WPARAM, LPARAM) { return 0; }
    inline int QueryFullProcessImageNameA(void*, unsigned long, char*, DWORD*) { return 0; }

    // Keyboard stubs
    inline unsigned int MapVirtualKeyA(unsigned int, unsigned int) { return 0; }
    inline int GetKeyNameTextA(long, char*, int) { return 0; }
    inline short VkKeyScanA(char) { return -1; }
    inline unsigned int MapVirtualKeyW(unsigned int, unsigned int) { return 0; }
    inline unsigned int MapVirtualKey(unsigned int, unsigned int) { return 0; }
    inline ULONG_PTR GetMessageExtraInfo() { return 0; }
    inline int GetDeviceCaps(void*, int) { return 96; }
    #define LOGPIXELSY 90
    #define SM_CYCURSOR 13
    inline int GetSystemMetricsForDpi(int, unsigned int) { return 0; }
    #define WM_GETDLGCODE 0x87

    // WinHTTP stubs (supporter fetch — cpp-httplib already available on Linux)
    using HINTERNET = void*;
    struct URL_COMPONENTS { unsigned long dwStructSize; wchar_t* lpszScheme; unsigned long dwSchemeLength; unsigned long nScheme; wchar_t* lpszHostName; unsigned long dwHostNameLength; unsigned short nPort; wchar_t* lpszUserName; unsigned long dwUserNameLength; wchar_t* lpszPassword; unsigned long dwPasswordLength; wchar_t* lpszUrlPath; unsigned long dwUrlPathLength; wchar_t* lpszExtraInfo; unsigned long dwExtraInfoLength; };
    #define INTERNET_SCHEME_HTTPS 2
    #define WINHTTP_FLAG_SECURE 0x800000
    #define WINHTTP_QUERY_STATUS_CODE 19
    #define WINHTTP_QUERY_FLAG_NUMBER 0x20000000
    #define WINHTTP_HEADER_NAME_BY_INDEX 0
    #define WINHTTP_NO_HEADER_INDEX 0xFFFFFFFF
    #define WINHTTP_QUERY_CONTENT_TYPE 1
    inline HINTERNET WinHttpOpen(const wchar_t*, unsigned long, const wchar_t*, const wchar_t*, unsigned long) { return nullptr; }
    inline HINTERNET WinHttpConnect(HINTERNET, const wchar_t*, unsigned short, unsigned long) { return nullptr; }
    inline HINTERNET WinHttpOpenRequest(HINTERNET, const wchar_t*, const wchar_t*, const wchar_t*, const wchar_t*, const wchar_t**, unsigned long) { return nullptr; }
    inline int WinHttpSetTimeouts(HINTERNET, int, int, int, int) { return 0; }
    inline int WinHttpSendRequest(HINTERNET, const wchar_t*, unsigned long, void*, unsigned long, unsigned long, unsigned long) { return 0; }
    inline int WinHttpReceiveResponse(HINTERNET, void*) { return 0; }
    inline int WinHttpQueryHeaders(HINTERNET, unsigned long, const wchar_t*, void*, unsigned long*, unsigned long*) { return 0; }
    inline int WinHttpQueryDataAvailable(HINTERNET, unsigned long*) { return 0; }
    inline int WinHttpReadData(HINTERNET, void*, unsigned long, unsigned long*) { return 0; }
    inline int WinHttpCloseHandle(HINTERNET) { return 0; }
    inline int WinHttpCrackUrl(const wchar_t*, unsigned long, unsigned long, URL_COMPONENTS*) { return 0; }

    // File dialog stubs
    struct OPENFILENAMEW { unsigned long lStructSize; void* hwndOwner; void* hInstance; const wchar_t* lpstrFilter; wchar_t* lpstrCustomFilter; unsigned long nMaxCustFilter; unsigned long nFilterIndex; wchar_t* lpstrFile; unsigned long nMaxFile; wchar_t* lpstrFileTitle; unsigned long nMaxFileTitle; const wchar_t* lpstrInitialDir; const wchar_t* lpstrTitle; unsigned long Flags; unsigned short nFileOffset, nFileExtension; const wchar_t* lpstrDefExt; LPARAM lCustData; void* lpfnHook; const wchar_t* lpTemplateName; void* pvReserved; unsigned long dwReserved; unsigned long FlagsEx; };
    #define OFN_PATHMUSTEXIST 0x800
    #define OFN_FILEMUSTEXIST 0x1000
    #define OFN_NOCHANGEDIR 8
    #define FNERR_INVALIDFILENAME 0x3002
    inline int GetOpenFileNameW(OPENFILENAMEW*) { return 0; }
    inline unsigned long CommDlgExtendedError() { return 0; }
    #define ZeroMemory(p,s) memset(p, 0, s)

    // Shell stubs
    #define SW_SHOWNORMAL 1
    using HINSTANCE = void*;
    inline HINSTANCE ShellExecuteW(void*, const wchar_t*, const wchar_t*, const wchar_t*, const wchar_t*, int) { return nullptr; }
    // GUID stub
    struct GUID { unsigned long Data1; unsigned short Data2, Data3; unsigned char Data4[8]; };
    static const GUID FOLDERID_Downloads = {0,0,0,{0,0,0,0,0,0,0,0}};
    #define KF_FLAG_DEFAULT 0
    inline int SHGetKnownFolderPath(const GUID&, unsigned long, void*, wchar_t**) { return 1; }
    inline void CoTaskMemFree(void*) {}

    // String stubs
    inline int sprintf_s(char* buf, const char* fmt, ...) { va_list va; va_start(va, fmt); vsprintf(buf, fmt, va); va_end(va); return 0; }
    // strncpy_s — 4-arg pointer version + 3-arg template version
    inline int strncpy_s(char* dst, size_t size, const char* src, size_t n) { if (dst && src) { strncpy(dst, src, std::min(size - 1, n)); dst[std::min(size - 1, n)] = '\0'; } return 0; }
    template<size_t N> inline int strncpy_s(char (&dst)[N], const char* src, size_t n) { return strncpy_s(dst, N, src, n); }
    inline int wcsncpy_s(wchar_t* dst, size_t size, const wchar_t* src, size_t n) { if (dst && src) { wcsncpy(dst, src, std::min(size - 1, n)); dst[std::min(size - 1, n)] = L'\0'; } return 0; }
    inline int _wcsicmp(const wchar_t* a, const wchar_t* b) { return wcscasecmp(a, b); }
    #define _TRUNCATE ((size_t)-1)
    using INT_PTR = intptr_t;
    using DWORD_PTR = uintptr_t;

    // Cursor stubs
    inline void SetCursor(void*) {}
    inline int ClipCursor(const RECT*) { return 1; }
    inline int GetClipCursor(RECT*) { return 1; }
    #define IDC_ARROW_MAKEINTRESOURCE reinterpret_cast<wchar_t*>(32512)

    // Keyboard layout stubs
    inline HKL GetKeyboardLayout(unsigned long) { return nullptr; }
    inline int ToUnicodeEx(unsigned int, unsigned int, const unsigned char*, wchar_t*, int, unsigned int, HKL) { return 0; }
    #define LANG_ENGLISH 0x09
    #define LOWORD(l) ((unsigned short)(l & 0xFFFF))
    inline unsigned int MapVirtualKeyEx(unsigned int, unsigned int, HKL) { return 0; }

    // XBUTTON/WHEEL macros
    #define GET_XBUTTON_WPARAM(w) ((unsigned short)(((w) >> 16) & 0xFFFF))
    #define ERROR_SHARING_VIOLATION 32L

    // WinHTTP extra stubs
    #define WINHTTP_NO_OUTPUT_BUFFER 0
    #define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
    #define WINHTTP_NO_PROXY_NAME nullptr
    #define WINHTTP_NO_PROXY_BYPASS nullptr
    #define WINHTTP_NO_REFERER nullptr
    #define WINHTTP_DEFAULT_ACCEPT_TYPES nullptr
    #define WINHTTP_NO_ADDITIONAL_HEADERS nullptr
    #define WINHTTP_NO_REQUEST_DATA nullptr
    // DWORD* overloads for WinHTTP
    inline int WinHttpQueryHeaders(HINTERNET, unsigned long, DWORD, void*, DWORD*, DWORD) { return 0; }
    // Overload for WINHTTP_NO_HEADER_INDEX usage
    inline int WinHttpQueryHeaders(HINTERNET, unsigned long, DWORD, void*, DWORD*, DWORD*) { return 0; }
    inline int WinHttpQueryDataAvailable(HINTERNET, DWORD*) { return 0; }
    inline int WinHttpReadData(HINTERNET, void*, unsigned long, DWORD*) { return 0; }

    // Resource stubs (needed by gui_runtime.cpp)
    inline int GetModuleHandleExW(unsigned long, const wchar_t*, HMODULE*) { return 0; }
    inline void* FindResourceW(HMODULE, const wchar_t*, const wchar_t*) { return nullptr; }
    inline void* LoadResource(HMODULE, void*) { return nullptr; }
    inline unsigned long SizeofResource(HMODULE, void*) { return 0; }
    inline const void* LockResource(void*) { return nullptr; }

    // ImGui API compatibility (v1.92.6 breaking changes)
    // These #defines are temporary workarounds; the code should be migrated properly.
    // SetWindowFontScale → removed, use GetIO().FontGlobalScale
    // GetWindowContentRegionMax → removed, use GetContentRegionAvail()
    // ImFontAtlas::Build → renamed to different signature
    #define GET_WHEEL_DELTA_WPARAM(w) ((short)(((w) >> 16) & 0xFFFF))
    #define GET_KEYSTATE_WPARAM(w) ((unsigned short)(w & 0xFFFF))
    #define GET_KEYSTATE_LPARAM(l) ((unsigned short)(l & 0xFFFF))
    #define GET_X_LPARAM(l) ((short)(l & 0xFFFF))
    #define GET_Y_LPARAM(l) ((short)(((l) >> 16) & 0xFFFF))
    inline void* SetCapture(void*) { return nullptr; }
    inline void* GetCapture() { return nullptr; }
    inline int ReleaseCapture() { return 1; }

    // Additional window stubs
    #define WM_MOUSEACTIVATE 0x21
    #define WM_NCHITTEST 0x84
    #define WM_SETCURSOR 0x20
    #define WM_IME_SETCONTEXT 0x281
    #define WM_IME_NOTIFY 0x282
    #define HTCLIENT 1
    #define SWP_NOSENDCHANGING 0x400

    // INPUT stub
    struct INPUT { unsigned long type; struct { unsigned short wVk, wScan; unsigned long dwFlags; unsigned long time; ULONG_PTR dwExtraInfo; } ki; };
    #define INPUT_KEYBOARD 1
    #define KEYEVENTF_KEYUP 2
    inline unsigned int SendInput(unsigned int, INPUT*, int) { return 0; }




    // errno_t
    using errno_t = int;
    #define FILE_SHARE_READ 1
    #define FILE_SHARE_WRITE 2
    #define FILE_SHARE_DELETE 4
    #define ERROR_ALREADY_EXISTS 183L
    #define OPEN_EXISTING 3
    inline void* CreateFileW(const wchar_t*, unsigned long, unsigned long, void*, unsigned long, unsigned long, void*) { return reinterpret_cast<void*>(-1); }
    inline int swprintf_s(wchar_t* buf, const wchar_t*, ...) { if (buf) buf[0] = L'\0'; return 0; }

    // GetCurrentProcessId polyfill
    inline unsigned long GetCurrentProcessId() { return static_cast<unsigned long>(::getpid()); }
    inline void* GetCurrentProcess() { return reinterpret_cast<void*>(static_cast<uintptr_t>(::getpid())); }

    // DbgHelp stubs (for compilation only; not functional on Linux)
    using DWORD64 = uint64_t;
    #define MAX_SYM_NAME 2000
    struct SYMBOL_INFO { unsigned long SizeOfStruct; unsigned long MaxNameLen; DWORD64 Address; char Name[1]; };
    using PSYMBOL_INFO = SYMBOL_INFO*;
    struct IMAGEHLP_LINE64 { unsigned long SizeOfStruct; DWORD64 Address; unsigned long LineNumber; char FileName[1]; };
    using TCHAR = char;

    // MultiByteToWideChar / WideCharToMultiByte polyfills (UTF-8 only)
    inline int MultiByteToWideChar(unsigned int /*CodePage*/, unsigned long /*dwFlags*/,
                                    const char* lpMultiByteStr, int cbMultiByte,
                                    wchar_t* lpWideCharStr, int cchWideChar) {
        if (!lpMultiByteStr || cbMultiByte == 0) return 0;
        std::string src(lpMultiByteStr, cbMultiByte < 0 ? strlen(lpMultiByteStr) : static_cast<size_t>(cbMultiByte));
        if (cchWideChar == 0) {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
            try { return static_cast<int>(conv.from_bytes(src).size()); }
            catch (...) { return 0; }
        }
        std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
        try {
            std::wstring result = conv.from_bytes(src);
            size_t len = std::min(static_cast<size_t>(cchWideChar) - 1, result.size());
            memcpy(lpWideCharStr, result.c_str(), len * sizeof(wchar_t));
            lpWideCharStr[len] = L'\0';
            return static_cast<int>(len);
        } catch (...) { return 0; }
    }

    inline int WideCharToMultiByte(unsigned int /*CodePage*/, unsigned long /*dwFlags*/,
                                    const wchar_t* lpWideCharStr, int cchWideChar,
                                    char* lpMultiByteStr, int cbMultiByte,
                                    const char* /*lpDefaultChar*/, int* /*lpUsedDefaultChar*/) {
        if (!lpWideCharStr || cchWideChar == 0) return 0;
        std::wstring src(lpWideCharStr, cchWideChar < 0 ? wcslen(lpWideCharStr) : static_cast<size_t>(cchWideChar));
        std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
        try {
            std::string result = conv.to_bytes(src);
            if (cbMultiByte == 0) return static_cast<int>(result.size());
            size_t len = std::min(static_cast<size_t>(cbMultiByte) - 1, result.size());
            memcpy(lpMultiByteStr, result.c_str(), len);
            lpMultiByteStr[len] = '\0';
            return static_cast<int>(len);
        } catch (...) { return 0; }
    }


    // getenv_s polyfill
    inline int getenv_s(size_t* requiredSize, char* buffer, size_t bufferSize, const char* name) {
        const char* val = ::getenv(name);
        if (!val) { if (requiredSize) *requiredSize = 0; return 1; }
        size_t len = strlen(val);
        if (requiredSize) *requiredSize = len;
        if (buffer && bufferSize > len) { strcpy(buffer, val); return 0; }
        return (buffer && bufferSize > 0) ? 1 : 0;
    }
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
