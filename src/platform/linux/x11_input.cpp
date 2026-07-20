#include "x11_input.h"
#include "x11_display.h"
#include "x11_cursor.h"

#ifdef PLATFORM_LINUX

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <unordered_set>

namespace X11Input {

namespace {

// Log to file (stderr is discarded by Minecraft's JVM)
static void X11_LOG(const char* fmt, ...) {
    FILE* f = fopen("/home/user/toolscreen.log", "a");
    if (f) { va_list va; va_start(va, fmt); vfprintf(f, fmt, va); va_end(va); fclose(f); }
}

EventCallback g_callback;
std::mutex g_callbackMutex;
std::atomic<bool> g_installed{false};
Window g_gameWindow = 0;

// XTEST availability (checked once, cached for all SendKey/SendChar calls)
std::atomic<bool> g_xtestChecked{false};
std::atomic<bool> g_xtestAvailable{false};

bool EnsureXTestAvailable() {
    // Thread-safe one-shot init (fixes race on g_xtestChecked / g_xtestAvailable)
    static std::once_flag xtestInitFlag;
    std::call_once(xtestInitFlag, []() {
        Display* dpy = X11Display::Get();
        if (!dpy) { g_xtestAvailable.store(false); return; }
        int major, minor;
        int evBase, errBase;
        bool available = XTestQueryExtension(dpy, &evBase, &errBase, &major, &minor);
        g_xtestAvailable.store(available, std::memory_order_release);
        if (!available) {
            X11_LOG("[Toolscreen] XTEST unavailable — synthetic input disabled\n");
        }
    });
    g_xtestChecked.store(true, std::memory_order_release);
    return g_xtestAvailable.load(std::memory_order_acquire);
}

// Track key state for modifier queries
std::mutex g_keyStateMutex;
std::unordered_map<uint32_t, bool> g_keyState;

// Map X11 keycode to canonical VK, handling AltGr, CapsLock, NumLock, and
// multiple keyboard groups (e.g., US+Russian layouts).
uint32_t XKeycodeToVk(Display* dpy, unsigned int keycode, unsigned int state) {
    // For key rebinding we need the PHYSICAL key identity regardless of
    // active keyboard layout, Shift, CapsLock, or AltGr state.
    // Always query group 0 level 0 — this gives the base (US-layout) keysym
    // for the physical key. The state parameter is forwarded to X11KeyToVk
    // for potential modifier-key disambiguation.
    KeySym keysym = XkbKeycodeToKeysym(dpy, keycode, 0, 0);
    if (keysym == NoSymbol) {
        return 0;
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
    X11_LOG("[Toolscreen] X11 input installed on window 0x%lx\n", gameWindow);
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

    // Use XCheckMaskEvent instead of XPending + XNextEvent to avoid a TOCTOU race:
    // if another thread reads the event between our check and XNextEvent,
    // XNextEvent blocks forever waiting for the next event.
    // XCheckMaskEvent is non-blocking — returns False immediately if no match.
    XEvent ev;
    while (XCheckMaskEvent(dpy, ~0, &ev)) {

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
            // Button4/5 (scroll wheel) generate events only on press,
            // not on release — otherwise scrolling would double-fire.
            if (ev.xbutton.button == Button4 || ev.xbutton.button == Button5)
                break;
            inputEv = XButtonEventToInputEvent(ev.xbutton, false);
            valid = true;
            break;
        case MotionNotify:
            inputEv = XMotionEventToInputEvent(ev.xmotion);
            valid = true;
            break;
        case FocusIn:
            inputEv.type = EventType::FocusGained;
            inputEv.window = ev.xfocus.window;
            valid = true;
            break;
        case FocusOut:
            inputEv.type = EventType::FocusLost;
            inputEv.window = ev.xfocus.window;
            valid = true;
            // Clear held-key state on focus loss to prevent "sticky" keys
            {
                std::lock_guard<std::mutex> lock(g_keyStateMutex);
                g_keyState.clear();
            }
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
        case PropertyNotify:
            // PropertyChange events are consumed without action;
            // the mask is included for compatibility with the game's event stream
            break;
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

    if (!EnsureXTestAvailable()) return;

    KeySym ks = X11Display::VkToX11Keysym(vkCode);
    if (ks == NoSymbol) return;

    unsigned int keycode = XKeysymToKeycode(dpy, ks);
    if (keycode == 0) {
        X11_LOG("[Toolscreen] SendKeyDown: no keycode for VK 0x%X\n", vkCode);
        return;
    }

    XTestFakeKeyEvent(dpy, keycode, True, CurrentTime);
    X11Display::Flush();
}

void SendKeyUp(uint32_t vkCode) {
    Display* dpy = X11Display::Get();
    if (!dpy || !g_gameWindow) return;

    if (!EnsureXTestAvailable()) return;

    KeySym ks = X11Display::VkToX11Keysym(vkCode);
    if (ks == NoSymbol) return;

    unsigned int keycode = XKeysymToKeycode(dpy, ks);
    if (keycode == 0) {
        X11_LOG("[Toolscreen] SendKeyUp: no keycode for VK 0x%X\n", vkCode);
        return;
    }

    XTestFakeKeyEvent(dpy, keycode, False, CurrentTime);
    X11Display::Flush();
}

void SendChar(uint32_t charCode) {
    if (charCode < 0x20 || charCode > 0x7E) return;

    Display* dpy = X11Display::Get();
    if (!dpy || !g_gameWindow) return;

    if (!EnsureXTestAvailable()) return;

    // Convert character to KeySym
    KeySym keysym = NoSymbol;
    if (charCode >= 'a' && charCode <= 'z') {
        keysym = XK_a + (charCode - 'a');
    } else if (charCode >= 'A' && charCode <= 'Z') {
        keysym = XK_A + (charCode - 'A');
    } else if (charCode >= '0' && charCode <= '9') {
        keysym = XK_0 + (charCode - '0');
    } else {
        // Symbols: use XStringToKeysym for layout-aware mapping
        char buf[2] = {static_cast<char>(charCode), '\0'};
        keysym = XStringToKeysym(buf);
    }

    if (keysym == NoSymbol) return;

    // Find the keycode that produces this keysym on the CURRENT layout
    unsigned int keycode = XKeysymToKeycode(dpy, keysym);
    if (keycode == 0) return;

    // Determine modifiers needed: check Shift (group 0 level 0 vs level 1)
    // and AltGr (group 1 level 0 — ISO_Level3_Shift on most layouts)
    KeySym baseKeysym = XkbKeycodeToKeysym(dpy, keycode, 0, 0);
    KeySym shiftKeysym = XkbKeycodeToKeysym(dpy, keycode, 0, 1);
    KeySym altgrKeysym = XkbKeycodeToKeysym(dpy, keycode, 1, 0);
    bool needsShift = (baseKeysym != keysym && shiftKeysym == keysym);
    bool needsAltGr = (altgrKeysym == keysym);

    unsigned int shiftKeycode = 0, altgrKeycode = 0;
    if (needsShift) {
        shiftKeycode = XKeysymToKeycode(dpy, XK_Shift_L);
        if (shiftKeycode) XTestFakeKeyEvent(dpy, shiftKeycode, True, CurrentTime);
    }
    if (needsAltGr) {
        altgrKeycode = XKeysymToKeycode(dpy, XK_ISO_Level3_Shift);
        if (altgrKeycode) XTestFakeKeyEvent(dpy, altgrKeycode, True, CurrentTime);
    }
    XTestFakeKeyEvent(dpy, keycode, True, CurrentTime);
    XTestFakeKeyEvent(dpy, keycode, False, CurrentTime);
    if (needsAltGr) {
        XTestFakeKeyEvent(dpy, altgrKeycode, False, CurrentTime);
    }
    if (needsShift) {
        XTestFakeKeyEvent(dpy, shiftKeycode, False, CurrentTime);
    }
    X11Display::Flush();
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
