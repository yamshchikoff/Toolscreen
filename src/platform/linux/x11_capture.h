#pragma once

#include "platform/platform_types.h"

#ifdef PLATFORM_LINUX

#include <X11/Xlib.h>
#include <vector>
#include <string>

// ---- X11 Screen and Window Capture ----

namespace X11Capture {

// Capture a screen region into RGBA8 buffer
// Uses XShmGetImage for fast shared-memory transfer
bool CaptureScreen(int x, int y, int w, int h,
                   std::vector<uint8_t>& outRgba,
                   int& outWidth, int& outHeight);

// Capture an X11 window into RGBA8 buffer
// Uses XComposite (if available) or XGetImage fallback
bool CaptureWindow(Window win, int& outWidth, int& outHeight,
                   std::vector<uint8_t>& outRgba);

// Check if a window is capturable (visible, not minimized, etc.)
bool IsWindowCapturable(Window win);

// Enumerate all visible top-level windows
struct WindowInfo {
    Window handle;
    std::string title;
    std::string wmClass;
    std::string executableName;
    int pid;
    int x, y, width, height;
    bool isVisible;
};

std::vector<WindowInfo> EnumerateWindows();

// Find a window by title, class, and/or executable name
Window FindWindowByTitleAndClass(const std::string& title,
                                  const std::string& wmClass,
                                  const std::string& executableName);

} // namespace X11Capture

#endif // PLATFORM_LINUX
