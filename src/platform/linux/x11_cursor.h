#pragma once

#include "platform/platform_types.h"

#ifdef PLATFORM_LINUX

#include <X11/Xlib.h>

// ---- X11 Cursor management ----
// Handles cursor visibility, confinement, and custom cursor textures.

namespace X11Cursor {

// Show or hide the system cursor
void ShowCursor(bool show);
bool IsCursorVisible();

// Confine cursor to a rectangle (for fullscreen cursor locking)
void ClipCursor(const PlatformRect* rect);

// Get current cursor position (global screen coordinates)
void GetCursorPos(int& outX, int& outY);

// Set cursor position (global screen coordinates)
void SetCursorPos(int x, int y);

// Load a custom cursor from image data (RGBA)
// Returns a cursor handle that can be used with SetCustomCursor
void* LoadCursorFromRGBA(const uint8_t* rgba, int width, int height, int hotX, int hotY);

// Set a custom cursor for the game window
void SetCustomCursor(Window win, void* cursorHandle);

// Free a custom cursor
void FreeCustomCursor(void* cursorHandle);

} // namespace X11Cursor

#endif // PLATFORM_LINUX
