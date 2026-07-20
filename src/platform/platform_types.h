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
    #include <cstring>
    #include <fstream>
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
    struct EXCEPTION_POINTERS;
    using LONG = int32_t;

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
    #define RT_RCDATA reinterpret_cast<void*>(10)

    // Win32 string type aliases
    using LPCWSTR = const wchar_t*;
    using LPCTSTR = const wchar_t*;
    using LPTSTR = wchar_t*;
    using LPWSTR = wchar_t*;
    using PWSTR = wchar_t*;
    using LPSTR = char*;

    // Additional Win32 types
    union LARGE_INTEGER { int64_t QuadPart; struct { int32_t LowPart; int32_t HighPart; } u; };
    struct IMAGE_CURSOR { int dummy; };  // Stub — Xcursor handles cursors on Linux
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
    struct IMAGE_ICON { int dummy; };
    inline int DestroyIcon(void*) { return 1; }
    inline void TerminateProcess(void*, unsigned int) { ::_exit(1); }
    inline unsigned long GetLastError() { return static_cast<unsigned long>(errno); }
    inline unsigned long GetCurrentThreadId() { return static_cast<unsigned long>(syscall(SYS_gettid)); }
    inline int GetSystemMetrics(int) { return 0; }
    #define SM_CXSCREEN 0
    #define SM_CYSCREEN 1

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
