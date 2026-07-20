#pragma once

#include "platform/platform_types.h"

#ifdef PLATFORM_LINUX

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <string>
#include <vector>
#include <functional>

// ---- X11 Input backend ----
// Handles keyboard, mouse, and hotkey events through X11 events and XInput2.

namespace X11Input {

// Input event types (mirrors Windows WM_* messages where possible)
enum class EventType {
    KeyDown,
    KeyUp,
    SysKeyDown,
    SysKeyUp,
    Char,
    MouseMove,
    MouseDown,
    MouseUp,
    MouseWheel,
    FocusGained,
    FocusLost,
    WindowResize,
    WindowMoved,
    WindowDestroy,
};

struct InputEvent {
    EventType type;
    Window window;
    uint64_t timestampMs;

    // Key
    uint32_t vkCode;      // Canonical VK_* code
    uint32_t scanCode;
    bool wasDown;

    // Char
    uint32_t charCode;

    // Mouse
    int mouseX, mouseY;
    int mouseDelta; // wheel

    // Raw motion
    int rawMouseDeltaX, rawMouseDeltaY;
};

// Callback: return true to consume (suppress) the event from the game
using EventCallback = std::function<bool(const InputEvent&)>;

// Install input interception on the game window
bool Install(Window gameWindow);

// Uninstall input hooks
void Uninstall();

// Set event callback (called for every input event before the game sees it)
void SetEventCallback(EventCallback cb);

// Poll pending X11 events and dispatch via callback
void PollEvents();

// ---- Low-level global keyboard hook equivalent ----
// On Linux, this intercepts X11 events before the game sees them
// by acting as a filter on the game window's event stream.

bool InstallLowLevelKeyboardHook();
void RemoveLowLevelKeyboardHook();

// Check if a key is currently held (via X11 QueryKeymap)
bool IsKeyDown(uint32_t vkCode);
bool IsShiftDown();
bool IsCtrlDown();
bool IsAltDown();

// ---- Synthetic input (for key rebind output) ----
// Uses XSendEvent + XTEST extension to inject keystrokes

void SendKeyDown(uint32_t vkCode);
void SendKeyUp(uint32_t vkCode);
void SendChar(uint32_t charCode);

// ---- Cursor position ----
void GetCursorPos(int& outX, int& outY);
void SetCursorPos(int x, int y);
void ShowCursor(bool show);

// ---- X11 event processing helpers ----
// Convert X11 KeyEvent to canonical InputEvent
InputEvent XKeyEventToInputEvent(const XKeyEvent& ev, bool isDown);

// Convert X11 ButtonEvent to canonical InputEvent
InputEvent XButtonEventToInputEvent(const XButtonEvent& ev, bool isDown);

// Convert X11 MotionEvent to canonical InputEvent
InputEvent XMotionEventToInputEvent(const XMotionEvent& ev);

} // namespace X11Input

#endif // PLATFORM_LINUX
