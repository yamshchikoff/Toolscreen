#include "x11_input.h"
#include "x11_display.h"
#include "x11_cursor.h"

#ifdef PLATFORM_LINUX

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <unordered_set>

namespace X11Input {

namespace {

EventCallback g_callback;
std::mutex g_callbackMutex;
std::atomic<bool> g_installed{false};
Window g_gameWindow = 0;

// Track key state for modifier queries
std::mutex g_keyStateMutex;
std::unordered_map<uint32_t, bool> g_keyState;

// Cursor visibility
std::atomic<bool> g_cursorVisible{true};

// Map X11 keycode to canonical VK, handling AltGr, CapsLock, NumLock, and
// multiple keyboard groups (e.g., US+Russian layouts).
uint32_t XKeycodeToVk(Display* dpy, unsigned int keycode, unsigned int state) {
    // Determine the effective keyboard group and shift level from modifier state
    // Mod5 (ISO_Level3_Shift) → group 1; Shift → level 1 within group
    int group = (state & Mod5Mask) ? 1 : 0;
    int level = (state & ShiftMask) ? 1 : 0;

    // Include LockMask so CapsLock affects the keysym correctly
    unsigned int mods = state & (ShiftMask | LockMask | Mod5Mask);

    KeySym keysym = XkbKeycodeToKeysym(dpy, keycode, group, level);
    if (keysym == NoSymbol) {
        keysym = XkbKeycodeToKeysym(dpy, keycode, 0, 0);
    }

    // Also try without LockMask if the result looks wrong for alpha keys
    if ((keysym < XK_A || keysym > XK_Z) && (keysym < XK_a || keysym > XK_z)) {
        KeySym keysymNoLock = XkbKeycodeToKeysym(dpy, keycode, group, level & ~1);
        if (keysymNoLock != NoSymbol &&
            ((keysymNoLock >= XK_A && keysymNoLock <= XK_Z) ||
             (keysymNoLock >= XK_a && keysymNoLock <= XK_z))) {
            keysym = keysymNoLock;
        }
    }

    return X11Display::X11KeyToVk(keysym, keycode, state);
}

} // namespace

bool Install(Window gameWindow) {
    if (g_installed.load()) return true;

    Display* dpy = X11Display::Get();
    if (!dpy || !gameWindow) return false;

    g_gameWindow = gameWindow;

    // Select input events on the game window, MERGING with the game's existing mask
    XWindowAttributes attrs;
    long existingMask = 0;
    if (XGetWindowAttributes(dpy, gameWindow, &attrs)) {
        existingMask = attrs.your_event_mask;
    }
    long ourMask = KeyPressMask | KeyReleaseMask |
                   ButtonPressMask | ButtonReleaseMask |
                   PointerMotionMask | ButtonMotionMask |
                   FocusChangeMask | StructureNotifyMask |
                   ExposureMask | PropertyChangeMask;
    XSelectInput(dpy, gameWindow, existingMask | ourMask);

    X11Display::Flush();
    g_installed.store(true);
    fprintf(stderr, "[Toolscreen] X11 input installed on window 0x%lx\n", gameWindow);
    return true;
}

void Uninstall() {
    g_installed.store(false);
    g_gameWindow = 0;
}

void SetEventCallback(EventCallback cb) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_callback = std::move(cb);
}

void PollEvents() {
    Display* dpy = X11Display::Get();
    if (!dpy || !g_installed.load()) return;

    while (XPending(dpy)) {
        XEvent ev;
        XNextEvent(dpy, &ev);

        InputEvent inputEv{};
        bool valid = false;

        switch (ev.type) {
        case KeyPress: {
            inputEv = XKeyEventToInputEvent(ev.xkey, true);
            valid = true;
            // Track key state
            {
                std::lock_guard<std::mutex> lock(g_keyStateMutex);
                g_keyState[inputEv.vkCode] = true;
            }
            break;
        }
        case KeyRelease: {
            inputEv = XKeyEventToInputEvent(ev.xkey, false);
            valid = true;
            {
                std::lock_guard<std::mutex> lock(g_keyStateMutex);
                g_keyState[inputEv.vkCode] = false;
            }
            break;
        }
        case ButtonPress:
            inputEv = XButtonEventToInputEvent(ev.xbutton, true);
            valid = true;
            break;
        case ButtonRelease:
            inputEv = XButtonEventToInputEvent(ev.xbutton, false);
            valid = true;
            break;
        case MotionNotify:
            inputEv = XMotionEventToInputEvent(ev.xmotion);
            valid = true;
            break;
        case FocusIn:
            inputEv.type = EventType::FocusIn;
            inputEv.window = ev.xfocus.window;
            valid = true;
            break;
        case FocusOut:
            inputEv.type = EventType::FocusOut;
            inputEv.window = ev.xfocus.window;
            valid = true;
            break;
        case ConfigureNotify:
            if (ev.xconfigure.width != 0 || ev.xconfigure.height != 0) {
                inputEv.type = EventType::WindowResize;
                inputEv.mouseX = ev.xconfigure.width;
                inputEv.mouseY = ev.xconfigure.height;
                valid = true;
            }
            break;
        case DestroyNotify:
            inputEv.type = EventType::WindowDestroy;
            inputEv.window = ev.xdestroywindow.window;
            valid = true;
            break;
        }

        if (valid) {
            std::lock_guard<std::mutex> lock(g_callbackMutex);
            if (g_callback) {
                g_callback(inputEv);
            }
        }
    }
}

bool InstallLowLevelKeyboardHook() {
    // On Linux, we don't have a true global keyboard hook without root.
    // Instead, we rely on our X11 event subscription on the game window,
    // which captures all keyboard events directed to the game.
    // For global hotkeys (e.g., while game is in background), we would
    // need XGrabKey on the root window.
    return true;
}

void RemoveLowLevelKeyboardHook() {
    // No cleanup needed
}

bool IsKeyDown(uint32_t vkCode) {
    std::lock_guard<std::mutex> lock(g_keyStateMutex);
    auto it = g_keyState.find(vkCode);
    return it != g_keyState.end() && it->second;
}

bool IsShiftDown() {
    return IsKeyDown(Vk::LSHIFT) || IsKeyDown(Vk::RSHIFT) || IsKeyDown(Vk::SHIFT);
}

bool IsCtrlDown() {
    return IsKeyDown(Vk::LCONTROL) || IsKeyDown(Vk::RCONTROL) || IsKeyDown(Vk::CONTROL);
}

bool IsAltDown() {
    return IsKeyDown(Vk::LMENU) || IsKeyDown(Vk::RMENU) || IsKeyDown(Vk::MENU);
}

void SendKeyDown(uint32_t vkCode) {
    Display* dpy = X11Display::Get();
    if (!dpy || !g_gameWindow) return;

    // Check XTEST availability
    int xtestMajor, xtestMinor;
    if (!XTestQueryExtension(dpy, &xtestMajor, &xtestMinor, nullptr, nullptr, nullptr)) {
        fprintf(stderr, "[Toolscreen] XTEST extension not available — synthetic input disabled\n");
        return;
    }

    KeySym ks = X11Display::VkToX11Keysym(vkCode);
    if (ks == NoSymbol) return;

    unsigned int keycode = XKeysymToKeycode(dpy, ks);
    if (keycode == 0) return;

    XTestFakeKeyEvent(dpy, keycode, True, CurrentTime);
    X11Display::Flush();
}

void SendKeyUp(uint32_t vkCode) {
    Display* dpy = X11Display::Get();
    if (!dpy || !g_gameWindow) return;

    int xtestMajor, xtestMinor;
    if (!XTestQueryExtension(dpy, &xtestMajor, &xtestMinor, nullptr, nullptr, nullptr)) return;

    KeySym ks = X11Display::VkToX11Keysym(vkCode);
    if (ks == NoSymbol) return;

    unsigned int keycode = XKeysymToKeycode(dpy, ks);
    if (keycode == 0) return;

    XTestFakeKeyEvent(dpy, keycode, False, CurrentTime);
    X11Display::Flush();
}

void SendChar(uint32_t charCode) {
    if (charCode < 0x20 || charCode > 0x7E) return;

    bool needsShift = false;
    PlatformVk vk = charCode;

    // Uppercase letters: simulate Shift + base key
    if (charCode >= 'A' && charCode <= 'Z') {
        needsShift = true;
        vk = charCode; // VK_A-VK_Z map to the base key
    }

    // Symbols that require Shift on US layout
    static const std::unordered_set<char> shiftSymbols = {
        '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
        '_', '+', '{', '}', '|', ':', '"', '<', '>', '?', '~'
    };
    if (shiftSymbols.count(static_cast<char>(charCode))) {
        needsShift = true;
    }

    if (needsShift) {
        SendKeyDown(Vk::LSHIFT);
    }
    SendKeyDown(vk);
    SendKeyUp(vk);
    if (needsShift) {
        SendKeyUp(Vk::LSHIFT);
    }
}

void GetCursorPos(int& outX, int& outY) {
    Display* dpy = X11Display::Get();
    if (!dpy) { outX = outY = 0; return; }

    Window root, child;
    int rootX, rootY, winX, winY;
    unsigned int mask;
    XQueryPointer(dpy, RootWindow(dpy, X11Display::GetScreen()),
                  &root, &child, &rootX, &rootY, &winX, &winY, &mask);
    outX = rootX;
    outY = rootY;
}

void SetCursorPos(int x, int y) {
    Display* dpy = X11Display::Get();
    if (!dpy) return;

    XWarpPointer(dpy, None, RootWindow(dpy, X11Display::GetScreen()),
                 0, 0, 0, 0, x, y);
    X11Display::Flush();
}

void ShowCursor(bool show) {
    // Delegate to X11Cursor to avoid duplicate invisible cursor
    X11Cursor::ShowCursor(show);
    g_cursorVisible.store(show);
}

// ---- X11 event conversion helpers ----

InputEvent XKeyEventToInputEvent(const XKeyEvent& ev, bool isDown) {
    InputEvent ie;
    ie.type = isDown ? EventType::KeyDown : EventType::KeyUp;
    ie.window = ev.window;
    ie.timestampMs = static_cast<uint64_t>(ev.time);

    Display* dpy = X11Display::Get();
    ie.vkCode = XKeycodeToVk(dpy, ev.keycode, ev.state);
    ie.scanCode = ev.keycode;

    return ie;
}

InputEvent XButtonEventToInputEvent(const XButtonEvent& ev, bool isDown) {
    InputEvent ie;
    ie.type = isDown ? EventType::MouseDown : EventType::MouseUp;
    ie.window = ev.window;
    ie.timestampMs = static_cast<uint64_t>(ev.time);
    ie.mouseX = ev.x;
    ie.mouseY = ev.y;

    switch (ev.button) {
    case Button1: ie.vkCode = Vk::MOUSE_LEFT; break;
    case Button2: ie.vkCode = Vk::MOUSE_MIDDLE; break;
    case Button3: ie.vkCode = Vk::MOUSE_RIGHT; break;
    case Button4: // Scroll up
        ie.type = EventType::MouseWheel;
        ie.mouseDelta = 120; // Positive = scroll up (matches Windows WHEEL_DELTA)
        break;
    case Button5: // Scroll down
        ie.type = EventType::MouseWheel;
        ie.mouseDelta = -120;
        break;
    default: ie.vkCode = 0; break;
    }

    return ie;
}

InputEvent XMotionEventToInputEvent(const XMotionEvent& ev) {
    InputEvent ie;
    ie.type = EventType::MouseMove;
    ie.window = ev.window;
    ie.timestampMs = static_cast<uint64_t>(ev.time);
    ie.mouseX = ev.x;
    ie.mouseY = ev.y;
    return ie;
}

} // namespace X11Input

#endif // PLATFORM_LINUX
