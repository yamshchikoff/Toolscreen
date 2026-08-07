#pragma once

#include "platform/platform_types.h"

#ifdef PLATFORM_LINUX

#include <X11/Xlib.h>
#include <string>
#include <vector>

namespace X11Window {

// ---- Window management ----

// Второе Display-соединение для ресайза в обход GLFW
void InitOwnDisplay();
void CloseOwnDisplay();

// Get the game's X11 window
Window GetGameWindow();

// Find the top-level WM-managed parent of a window.
// Walks up XQueryTree until it finds a window with WM_STATE set.
Window FindTopLevelWindow(Display* dpy, Window win);

// Set the game window (called after detection)
void SetGameWindow(Window win);

// Check if a window is valid (still exists and is mapped)
bool IsWindowValid(Window win);

// Check if the game window is in the foreground (has focus)
bool IsWindowInForeground(Window win);

// Get window client rectangle in screen coordinates
bool GetWindowClientRect(Window win, PlatformRect& outRect);

// Get the monitor geometry containing the window
bool GetMonitorRectForWindow(Window win, PlatformRect& outRect);
bool GetMonitorSizeForWindow(Window win, int& outW, int& outH);

// Request a window resize (sets WM_NORMAL_HINTS then XResizeWindow)
bool RequestWindowResize(Window win, int width, int height);

// Toggle fullscreen / borderless windowed mode
bool SetBorderlessFullscreen(Window win, bool borderless);

// Get window attributes
bool GetWindowAttributes(Window win, XWindowAttributes& outAttrs);

// Get the current screen (monitor) dimensions
void GetScreenSize(int& outW, int& outH);

// Set the window title
void SetWindowTitle(Window win, const std::string& title);

// Check if cursor is currently visible in the window
bool IsCursorVisible();

// Enum all monitors
struct MonitorInfo {
    int index;
    PlatformRect rect;
    bool isPrimary;
};
std::vector<MonitorInfo> GetMonitors();

} // namespace X11Window

#endif // PLATFORM_LINUX
