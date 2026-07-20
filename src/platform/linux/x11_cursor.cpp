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

// Lazily-created invisible cursor for cursor hiding
Cursor g_invisibleCursor = 0;
std::mutex g_invisibleCursorMutex;

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
        std::lock_guard<std::mutex> lock(g_invisibleCursorMutex);
        if (!g_invisibleCursor) {
            XColor dummy;
            memset(&dummy, 0, sizeof(dummy));
            Pixmap blank = XCreatePixmap(dpy, win, 1, 1, 1);
            g_invisibleCursor = XCreatePixmapCursor(dpy, blank, blank, &dummy, &dummy, 0, 0);
            XFreePixmap(dpy, blank);
        }
        XDefineCursor(dpy, win, g_invisibleCursor);
    }

    g_cursorVisible.store(show);
    X11Display::Flush();
}

void Shutdown() {
    Display* dpy = X11Display::Get();
    if (dpy && g_invisibleCursor) {
        XFreeCursor(dpy, g_invisibleCursor);
        g_invisibleCursor = 0;
    }
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
        // event_mask=0: we only want to confine the pointer position,
        // not intercept mouse events from the game window.
        int result = XGrabPointer(dpy, confineWin, True,
                                   0,
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

    image->data = new (std::nothrow) char[image->bytes_per_line * height];
    if (!image->data) {
        XDestroyImage(image);
        return nullptr;
    }
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
    if (!gc) { delete[] image->data; image->data = nullptr; XDestroyImage(image); XFreePixmap(dpy, pixmap); return nullptr; }
    XPutImage(dpy, pixmap, gc, image, 0, 0, 0, 0, width, height);

    // Build 1-bit mask bitmap in memory (avoids O(w*h) X server round-trips)
    int bytesPerRow = (width + 7) / 8;
    std::vector<uint8_t> maskBits(bytesPerRow * height, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t srcIdx = (static_cast<size_t>(y) * width + x) * 4;
            if (rgba[srcIdx + 3] > 128) {
                maskBits[y * bytesPerRow + x / 8] |= (0x80 >> (x % 8));
            }
        }
    }
    Pixmap mask = XCreatePixmapFromBitmapData(dpy, X11Display::GetRoot(),
                                                reinterpret_cast<char*>(maskBits.data()),
                                                width, height, 1, 0, 1);
    if (!mask) {
        // XCreatePixmapFromBitmapData failed (extremely rare — X server OOM).
        // Fall back to DrawPoint loop so the mask is correct rather than empty.
        mask = XCreatePixmap(dpy, X11Display::GetRoot(), width, height, 1);
        GC maskGc = XCreateGC(dpy, mask, 0, nullptr);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                size_t srcIdx = (static_cast<size_t>(y) * width + x) * 4;
                XSetForeground(dpy, maskGc, rgba[srcIdx + 3] > 128 ? 1 : 0);
                XDrawPoint(dpy, mask, maskGc, x, y);
            }
        }
        XFreeGC(dpy, maskGc);
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
