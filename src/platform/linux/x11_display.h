#pragma once

#include "platform/platform_types.h"

// ---- X11 Display singleton ----
// Manages the shared X11 Display connection and provides GLX context helpers.

#ifdef PLATFORM_LINUX

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#include <string>
#include <functional>

namespace X11Display {

// Open the shared X11 display (connects to $DISPLAY or :0)
bool Open();

// Close the display connection
void Close();

// Get the shared Display pointer (nullptr if not open)
Display* Get();

// Get the default screen number
int GetScreen();

// Get the root window
Window GetRoot();

// Find a window by its LWJGL/X11 properties (used to identify the game window)
Window FindGameWindow();

// Set the game window (called after initial detection)
void SetGameWindow(Window win);
Window GetGameWindow();

// Get a window's geometry
bool GetWindowGeometry(Window win, PlatformRect& outRect);

// Get a window's title
std::string GetWindowTitle(Window win);

// Get the current GLX context
GLXContext GetCurrentContext();

// Get the current GLX drawable
GLXDrawable GetCurrentDrawable();

// Get the current display (from GLX context)
Display* GetCurrentDisplay();

// Check if we're on the game's render thread
bool IsOnRenderThread();

// Flush pending X11 commands
void Flush();

// Get the number of monitors/screens
int GetMonitorCount();

// Get monitor geometry by index
bool GetMonitorGeometry(int index, PlatformRect& outRect, bool& outIsPrimary);

// Convert X11 keycode + state to canonical VK
uint32_t X11KeyToVk(KeySym keysym, unsigned int keycode, unsigned int state);

// Convert canonical VK to X11 keysym
KeySym VkToX11Keysym(PlatformVk vk);

// Get a printable string for a keycode/keysym
std::string KeysymToString(KeySym keysym);

} // namespace X11Display

#endif // PLATFORM_LINUX
