#include "x11_window.h"
#include "x11_display.h"
#include "x11_capture.h"

#ifdef PLATFORM_LINUX

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace X11Window {

namespace {

std::mutex g_windowMutex;
Window g_gameWindow = 0;

// EWMH atoms
Atom g_netWmState = None;
Atom g_netWmStateFullscreen = None;
Atom g_netWmStateHidden = None;
Atom g_netMoveResize = None;

void InitAtoms() {
    Display* dpy = X11Display::Get();
    if (!dpy) return;

    g_netWmState = XInternAtom(dpy, "_NET_WM_STATE", False);
    g_netWmStateFullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    g_netWmStateHidden = XInternAtom(dpy, "_NET_WM_STATE_HIDDEN", False);
    g_netMoveResize = XInternAtom(dpy, "_NET_MOVERESIZE_WINDOW", False);
}

} // namespace

Window GetGameWindow() {
    std::lock_guard<std::mutex> lock(g_windowMutex);
    return g_gameWindow;
}

void SetGameWindow(Window win) {
    std::lock_guard<std::mutex> lock(g_windowMutex);
    g_gameWindow = win;
    InitAtoms();
}

bool IsWindowValid(Window win) {
    Display* dpy = X11Display::Get();
    if (!dpy || !win) return false;

    XWindowAttributes attrs;
    return XGetWindowAttributes(dpy, win, &attrs) != 0;
}

bool IsWindowInForeground(Window win) {
    Display* dpy = X11Display::Get();
    if (!dpy || !win) return false;

    Window focused;
    int revertTo;
    XGetInputFocus(dpy, &focused, &revertTo);

    // Check if our window or any of its children has focus
    if (focused == win) return true;

    // Walk up the window tree
    Window root, parent, *children = nullptr;
    unsigned int nchildren = 0;
    Window current = focused;
    for (int i = 0; i < 20; ++i) { // Max depth
        if (current == win) return true;
        if (current == 0) break;
        if (!XQueryTree(dpy, current, &root, &parent, &children, &nchildren) || !children) break;
        XFree(children);
        children = nullptr;
        current = parent;
    }
    return false;
}

bool GetWindowClientRect(Window win, PlatformRect& outRect) {
    Display* dpy = X11Display::Get();
    if (!dpy || !win) return false;

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, win, &attrs)) return false;

    Window child;
    int x, y;
    XTranslateCoordinates(dpy, win, attrs.root, 0, 0, &x, &y, &child);

    outRect.left = x;
    outRect.top = y;
    outRect.right = x + attrs.width;
    outRect.bottom = y + attrs.height;
    return true;
}

bool GetMonitorRectForWindow(Window win, PlatformRect& outRect) {
    Display* dpy = X11Display::Get();
    if (!dpy || !win) return false;

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, win, &attrs)) return false;

    // Get monitor from window center
    Window child;
    int cx, cy;
    XTranslateCoordinates(dpy, win, attrs.root, attrs.width / 2, attrs.height / 2,
                         &cx, &cy, &child);

    // Use XRandR to find the monitor containing (cx, cy)
    int monitorCount = X11Display::GetMonitorCount();
    for (int i = 0; i < monitorCount; ++i) {
        bool isPrimary;
        PlatformRect r;
        if (X11Display::GetMonitorGeometry(i, r, isPrimary)) {
            if (cx >= r.left && cx < r.right && cy >= r.top && cy < r.bottom) {
                outRect = r;
                return true;
            }
        }
    }

    // Fallback: use default screen
    Screen* screen = ScreenOfDisplay(dpy, X11Display::GetScreen());
    outRect.left = 0;
    outRect.top = 0;
    outRect.right = WidthOfScreen(screen);
    outRect.bottom = HeightOfScreen(screen);
    return true;
}

bool GetMonitorSizeForWindow(Window win, int& outW, int& outH) {
    PlatformRect rect;
    if (GetMonitorRectForWindow(win, rect)) {
        outW = rect.width();
        outH = rect.height();
        return true;
    }
    return false;
}

bool RequestWindowResize(Window win, int width, int height) {
    Display* dpy = X11Display::Get();
    if (!dpy || !win) return false;

    InitAtoms();

    // Попытка 5: XConfigureWindow — прямое изменение размера окна
    XWindowChanges changes;
    changes.width = width;
    changes.height = height;
    XConfigureWindow(dpy, win, CWWidth | CWHeight, &changes);
    X11Display::Flush();
    return true;
}

bool SetBorderlessFullscreen(Window win, bool borderless) {
    Display* dpy = X11Display::Get();
    if (!dpy || !win) return false;
    InitAtoms();
    if (g_netWmState == None || g_netWmStateFullscreen == None) return false;

    if (borderless) {
        // Set _NET_WM_STATE_FULLSCREEN
        XEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = ClientMessage;
        ev.xclient.window = win;
        ev.xclient.message_type = g_netWmState;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
        ev.xclient.data.l[1] = g_netWmStateFullscreen;
        ev.xclient.data.l[2] = 0;

        XSendEvent(dpy, X11Display::GetRoot(), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    } else {
        // Remove _NET_WM_STATE_FULLSCREEN
        XEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = ClientMessage;
        ev.xclient.window = win;
        ev.xclient.message_type = g_netWmState;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE
        ev.xclient.data.l[1] = g_netWmStateFullscreen;
        ev.xclient.data.l[2] = 0;

        XSendEvent(dpy, X11Display::GetRoot(), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    }

    X11Display::Flush();
    return true;
}

bool GetWindowAttributes(Window win, XWindowAttributes& outAttrs) {
    Display* dpy = X11Display::Get();
    if (!dpy || !win) return false;
    return XGetWindowAttributes(dpy, win, &outAttrs) != 0;
}

void GetScreenSize(int& outW, int& outH) {
    Display* dpy = X11Display::Get();
    if (!dpy) { outW = outH = 0; return; }

    Screen* screen = ScreenOfDisplay(dpy, X11Display::GetScreen());
    outW = WidthOfScreen(screen);
    outH = HeightOfScreen(screen);
}

void SetWindowTitle(Window win, const std::string& title) {
    Display* dpy = X11Display::Get();
    if (!dpy || !win) return;

    XStoreName(dpy, win, title.c_str());
    X11Display::Flush();
}

// NOTE: different from X11Cursor::IsCursorVisible() — this checks
// whether the pointer is physically inside the window (XQueryPointer),
// while X11Cursor::IsCursorVisible() returns the logical show/hide flag.
bool IsCursorVisible() {
    Display* dpy = X11Display::Get();
    if (!dpy) return true;

    Window root, child;
    int rootX, rootY, winX, winY;
    unsigned int mask;
    Bool result = XQueryPointer(dpy, X11Display::GetRoot(),
                                &root, &child, &rootX, &rootY,
                                &winX, &winY, &mask);
    return result != False;
}

std::vector<MonitorInfo> GetMonitors() {
    std::vector<MonitorInfo> result;
    Display* dpy = X11Display::Get();
    if (!dpy) return result;

    int count = X11Display::GetMonitorCount();
    for (int i = 0; i < count; ++i) {
        MonitorInfo info;
        info.index = i;
        bool primary;
        if (X11Display::GetMonitorGeometry(i, info.rect, primary)) {
            info.isPrimary = primary;
            result.push_back(info);
        }
    }
    return result;
}

} // namespace X11Window

#endif // PLATFORM_LINUX
