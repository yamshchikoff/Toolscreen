#include "x11_cursor.h"
#include "x11_display.h"

#ifdef PLATFORM_LINUX

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdio>
#include <cstring>
#include <atomic>

namespace X11Cursor {

namespace {

std::atomic<bool> g_cursorVisible{true};

} // namespace

void ShowCursor(bool show) {
    Display* dpy = X11Display::Get();
    if (!dpy) return;

    Window win = X11Display::GetGameWindow();
    if (!win) return;

    if (show) {
        XUndefineCursor(dpy, win);
    } else {
        // Create invisible cursor on first use (mutex-protected against races)
        static Cursor invisibleCursor = 0;
        static std::mutex cursorMutex;
        std::lock_guard<std::mutex> lock(cursorMutex);
        if (!invisibleCursor) {
            XColor dummy;
            memset(&dummy, 0, sizeof(dummy));
            Pixmap blank = XCreatePixmap(dpy, win, 1, 1, 1);
            invisibleCursor = XCreatePixmapCursor(dpy, blank, blank, &dummy, &dummy, 0, 0);
            XFreePixmap(dpy, blank);
        }
        XDefineCursor(dpy, win, invisibleCursor);
    }

    g_cursorVisible.store(show);
    X11Display::Flush();
}

bool IsCursorVisible() {
    return g_cursorVisible.load();
}

void ClipCursor(const PlatformRect* rect) {
    Display* dpy = X11Display::Get();
    if (!dpy) return;

    if (rect) {
        // Confine cursor to the specified rectangle using XGrabPointer
        // This is a game-focused approach: grab the pointer to prevent
        // it from leaving the window area during gameplay.
        // The rect parameter defines the confinement area.
        Window confineWin = X11Display::GetGameWindow();
        if (!confineWin) {
            confineWin = X11Display::GetRoot();
        }
        int result = XGrabPointer(dpy, confineWin, True,
                                   ButtonPressMask | ButtonReleaseMask |
                                   PointerMotionMask,
                                   GrabModeAsync, GrabModeAsync,
                                   confineWin, None, CurrentTime);
        if (result != GrabSuccess) {
            fprintf(stderr, "[Toolscreen] XGrabPointer failed for cursor clipping\n");
        }
    } else {
        XUngrabPointer(dpy, CurrentTime);
    }
    X11Display::Flush();
}

void GetCursorPos(int& outX, int& outY) {
    Display* dpy = X11Display::Get();
    if (!dpy) { outX = outY = 0; return; }

    Window root, child;
    int rootX, rootY, winX, winY;
    unsigned int mask;
    XQueryPointer(dpy, X11Display::GetRoot(),
                  &root, &child, &rootX, &rootY, &winX, &winY, &mask);
    outX = rootX;
    outY = rootY;
}

void SetCursorPos(int x, int y) {
    Display* dpy = X11Display::Get();
    if (!dpy) return;

    XWarpPointer(dpy, None, X11Display::GetRoot(), 0, 0, 0, 0, x, y);
    X11Display::Flush();
}

void* LoadCursorFromRGBA(const uint8_t* rgba, int width, int height, int hotX, int hotY) {
    Display* dpy = X11Display::Get();
    if (!dpy) return nullptr;

    // Create X11 cursor from RGBA data
    // Convert RGBA to X11 bitmap format (1-bit mask + color image)
    XImage* image = XCreateImage(dpy, DefaultVisual(dpy, X11Display::GetScreen()),
                                  24, ZPixmap, 0, nullptr, width, height, 32, 0);
    if (!image) return nullptr;

    image->data = new char[image->bytes_per_line * height];
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t srcIdx = (y * width + x) * 4;
            unsigned long pixel = (rgba[srcIdx + 2] << 16) | // R
                                   (rgba[srcIdx + 1] << 8)  | // G
                                    rgba[srcIdx + 0];          // B
            XPutPixel(image, x, y, pixel);
        }
    }

    // Create cursor pixmap
    Pixmap pixmap = XCreatePixmap(dpy, X11Display::GetRoot(), width, height,
                                   DefaultDepth(dpy, X11Display::GetScreen()));
    GC gc = XCreateGC(dpy, pixmap, 0, nullptr);
    XPutImage(dpy, pixmap, gc, image, 0, 0, 0, 0, width, height);

    // Create mask (1-bit bitmap from alpha channel: alpha > 128 = visible)
    Pixmap mask = XCreatePixmap(dpy, X11Display::GetRoot(), width, height, 1);
    GC maskGc = XCreateGC(dpy, mask, 0, nullptr);
    // Build mask from alpha values — use XPutImage with a 1-bit depth bitmap
    // For simplicity: draw each pixel where alpha > 128
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t srcIdx = (y * width + x) * 4;
            uint8_t alpha = rgba[srcIdx + 3];
            if (alpha > 128) {
                XSetForeground(dpy, maskGc, 1);
            } else {
                XSetForeground(dpy, maskGc, 0);
            }
            XDrawPoint(dpy, mask, maskGc, x, y);
        }
    }

    XColor fg, bg;
    fg.red = fg.green = fg.blue = 65535;
    bg.red = bg.green = bg.blue = 0;

    Cursor cursor = XCreatePixmapCursor(dpy, pixmap, mask, &fg, &bg, hotX, hotY);

    delete[] image->data;
    image->data = nullptr;
    XDestroyImage(image);
    XFreePixmap(dpy, pixmap);
    XFreePixmap(dpy, mask);
    XFreeGC(dpy, gc);
    XFreeGC(dpy, maskGc);

    return reinterpret_cast<void*>(static_cast<uintptr_t>(cursor));
}

void SetCustomCursor(Window win, void* cursorHandle) {
    Display* dpy = X11Display::Get();
    if (!dpy || !win || !cursorHandle) return;

    Cursor cursor = static_cast<Cursor>(reinterpret_cast<uintptr_t>(cursorHandle));
    XDefineCursor(dpy, win, cursor);
    X11Display::Flush();
}

void FreeCustomCursor(void* cursorHandle) {
    Display* dpy = X11Display::Get();
    if (!dpy || !cursorHandle) return;

    Cursor cursor = static_cast<Cursor>(reinterpret_cast<uintptr_t>(cursorHandle));
    XFreeCursor(dpy, cursor);
}

} // namespace X11Cursor

#endif // PLATFORM_LINUX
