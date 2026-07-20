#include "x11_capture.h"
#include "x11_display.h"

#ifdef PLATFORM_LINUX

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/XShm.h>
#include <sys/shm.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <mutex>

namespace X11Capture {

namespace {

bool g_shmAvailable = false;

bool InitShm() {
    Display* dpy = X11Display::Get();
    if (!dpy) return false;

    int major, minor;
    Bool pixmaps;
    if (XShmQueryVersion(dpy, &major, &minor, &pixmaps)) {
        g_shmAvailable = true;
        fprintf(stderr, "[Toolscreen] XShm available (v%d.%d, pixmaps=%d)\n", major, minor, pixmaps);
        return true;
    }
    return false;
}

} // namespace

bool CaptureScreen(int x, int y, int w, int h,
                   std::vector<uint8_t>& outRgba,
                   int& outWidth, int& outHeight) {
    Display* dpy = X11Display::Get();
    if (!dpy) return false;

    // Ensure SHM is initialized
    static bool shmTried = false;
    if (!shmTried) {
        shmTried = true;
        InitShm();
    }

    Window root = X11Display::GetRoot();
    if (!root) return false;

    if (w <= 0 || h <= 0) {
        Screen* screen = ScreenOfDisplay(dpy, X11Display::GetScreen());
        w = WidthOfScreen(screen);
        h = HeightOfScreen(screen);
        x = 0;
        y = 0;
    }

    outWidth = w;
    outHeight = h;
    outRgba.resize(w * h * 4);

    if (g_shmAvailable) {
        // XShm path — fast shared memory transfer
        XShmSegmentInfo shmInfo;
        XImage* image = XShmCreateImage(dpy, DefaultVisual(dpy, X11Display::GetScreen()),
                                         DefaultDepth(dpy, X11Display::GetScreen()),
                                         ZPixmap, nullptr, &shmInfo, w, h);
        if (!image) goto fallback;

        shmInfo.shmid = shmget(IPC_PRIVATE, image->bytes_per_line * image->height,
                              IPC_CREAT | 0666);
        if (shmInfo.shmid == -1) {
            XDestroyImage(image);
            goto fallback;
        }

        shmInfo.shmaddr = image->data = static_cast<char*>(shmat(shmInfo.shmid, nullptr, 0));
        if (shmInfo.shmaddr == reinterpret_cast<void*>(-1)) {
            XDestroyImage(image);
            shmctl(shmInfo.shmid, IPC_RMID, nullptr);
            goto fallback;
        }
        // Mark segment for deletion early — Linux defers until last detach
        shmctl(shmInfo.shmid, IPC_RMID, nullptr);
        shmInfo.readOnly = False;
        XShmAttach(dpy, &shmInfo);

        XShmGetImage(dpy, root, image, x, y, AllPlanes);

        // Convert to RGBA8 (X11 image is typically BGRX or BGRA depending on depth)
        int depth = DefaultDepth(dpy, X11Display::GetScreen());
        uint8_t* src = reinterpret_cast<uint8_t*>(image->data);
        for (int row = 0; row < h; ++row) {
            for (int col = 0; col < w; ++col) {
                size_t srcIdx = row * image->bytes_per_line + col * (image->bits_per_pixel / 8);
                size_t dstIdx = (row * w + col) * 4;

                if (depth >= 24) {
                    outRgba[dstIdx + 0] = src[srcIdx + 2]; // R (from BGR)
                    outRgba[dstIdx + 1] = src[srcIdx + 1]; // G
                    outRgba[dstIdx + 2] = src[srcIdx + 0]; // B
                    outRgba[dstIdx + 3] = 255;             // A
                } else {
                    // 16-bit or lower — use XGetPixel
                    unsigned long pixel = XGetPixel(image, col, row);
                    outRgba[dstIdx + 0] = (pixel >> 16) & 0xFF;
                    outRgba[dstIdx + 1] = (pixel >> 8) & 0xFF;
                    outRgba[dstIdx + 2] = pixel & 0xFF;
                    outRgba[dstIdx + 3] = 255;
                }
            }
        }

        XShmDetach(dpy, &shmInfo);
        XDestroyImage(image);
        shmdt(shmInfo.shmaddr);
        return true;
    }

fallback:
    // XGetImage fallback — slower but always works
    XImage* image = XGetImage(dpy, root, x, y, w, h, AllPlanes, ZPixmap);
    if (!image) return false;

    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            size_t dstIdx = (row * w + col) * 4;
            unsigned long pixel = XGetPixel(image, col, row);
            outRgba[dstIdx + 0] = (pixel >> 16) & 0xFF;
            outRgba[dstIdx + 1] = (pixel >> 8) & 0xFF;
            outRgba[dstIdx + 2] = pixel & 0xFF;
            outRgba[dstIdx + 3] = 255;
        }
    }

    XDestroyImage(image);
    return true;
}

bool CaptureWindow(Window win, int& outWidth, int& outHeight,
                   std::vector<uint8_t>& outRgba) {
    Display* dpy = X11Display::Get();
    if (!dpy || !win) return false;

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, win, &attrs)) return false;

    outWidth = attrs.width;
    outHeight = attrs.height;

    if (outWidth <= 0 || outHeight <= 0) return false;

    XImage* image = XGetImage(dpy, win, 0, 0, outWidth, outHeight, AllPlanes, ZPixmap);
    if (!image) return false;

    outRgba.resize(outWidth * outHeight * 4);

    for (int row = 0; row < outHeight; ++row) {
        for (int col = 0; col < outWidth; ++col) {
            size_t dstIdx = (row * outWidth + col) * 4;
            unsigned long pixel = XGetPixel(image, col, row);
            outRgba[dstIdx + 0] = (pixel >> 16) & 0xFF;
            outRgba[dstIdx + 1] = (pixel >> 8) & 0xFF;
            outRgba[dstIdx + 2] = pixel & 0xFF;
            outRgba[dstIdx + 3] = 255;
        }
    }

    XDestroyImage(image);
    return true;
}

bool IsWindowCapturable(Window win) {
    Display* dpy = X11Display::Get();
    if (!dpy || !win) return false;

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, win, &attrs)) return false;

    if (attrs.map_state != IsViewable) return false;

    // Check _NET_WM_STATE for Hidden
    Atom wmState = XInternAtom(dpy, "_NET_WM_STATE", True);
    Atom hidden = XInternAtom(dpy, "_NET_WM_STATE_HIDDEN", True);
    if (wmState != None && hidden != None) {
        Atom actualType;
        int actualFormat;
        unsigned long nitems, bytesAfter;
        unsigned char* prop = nullptr;
        if (XGetWindowProperty(dpy, win, wmState, 0, 64, False, XA_ATOM,
                               &actualType, &actualFormat, &nitems, &bytesAfter, &prop) == Success && prop) {
            Atom* atoms = reinterpret_cast<Atom*>(prop);
            for (unsigned long i = 0; i < nitems; ++i) {
                if (atoms[i] == hidden) {
                    XFree(prop);
                    return false;
                }
            }
            XFree(prop);
        }
    }

    return attrs.width > 0 && attrs.height > 0;
}

std::vector<WindowInfo> EnumerateWindows() {
    std::vector<WindowInfo> result;
    Display* dpy = X11Display::Get();
    if (!dpy) return result;

    Atom netClientList = XInternAtom(dpy, "_NET_CLIENT_LIST", True);
    Atom netWmName = XInternAtom(dpy, "_NET_WM_NAME", True);
    Atom wmClassAtom = XInternAtom(dpy, "WM_CLASS", True);
    Atom wmPid = XInternAtom(dpy, "_NET_WM_PID", True);

    if (netClientList != None) {
        Atom actualType;
        int actualFormat;
        unsigned long nitems, bytesAfter;
        unsigned char* prop = nullptr;

        if (XGetWindowProperty(dpy, X11Display::GetRoot(), netClientList, 0, ~0UL,
                               False, XA_WINDOW, &actualType, &actualFormat,
                               &nitems, &bytesAfter, &prop) == Success && prop) {
            Window* windows = reinterpret_cast<Window*>(prop);
            for (unsigned long i = 0; i < nitems; ++i) {
                WindowInfo info;
                info.handle = windows[i];

                // Get name
                unsigned char* nameProp = nullptr;
                if (XGetWindowProperty(dpy, windows[i], netWmName, 0, 1024, False,
                                       AnyPropertyType, &actualType, &actualFormat,
                                       &nitems, &bytesAfter, &nameProp) == Success && nameProp) {
                    info.title = std::string(reinterpret_cast<char*>(nameProp));
                    XFree(nameProp);
                }

                // Get class
                if (XGetWindowProperty(dpy, windows[i], wmClassAtom, 0, 1024, False,
                                       AnyPropertyType, &actualType, &actualFormat,
                                       &nitems, &bytesAfter, &nameProp) == Success && nameProp) {
                    info.wmClass = std::string(reinterpret_cast<char*>(nameProp));
                    XFree(nameProp);
                }

                // Get PID
                if (XGetWindowProperty(dpy, windows[i], wmPid, 0, 1, False,
                                       XA_CARDINAL, &actualType, &actualFormat,
                                       &nitems, &bytesAfter, &nameProp) == Success && nameProp) {
                    info.pid = *reinterpret_cast<uint32_t*>(nameProp);
                    XFree(nameProp);
                }

                // Get geometry
                XWindowAttributes attrs;
                if (XGetWindowAttributes(dpy, windows[i], &attrs)) {
                    Window child;
                    XTranslateCoordinates(dpy, windows[i], attrs.root, 0, 0,
                                         &info.x, &info.y, &child);
                    info.width = attrs.width;
                    info.height = attrs.height;
                    info.isVisible = (attrs.map_state == IsViewable);
                }

                if (info.isVisible) result.push_back(std::move(info));
            }
            XFree(prop);
        }
    }

    return result;
}

Window FindWindowByTitleAndClass(const std::string& title,
                                  const std::string& wmClass,
                                  const std::string& executableName) {
    auto windows = EnumerateWindows();

    for (auto& w : windows) {
        bool titleMatch = !title.empty() && w.title.find(title) != std::string::npos;
        bool classMatch = !wmClass.empty() && w.wmClass.find(wmClass) != std::string::npos;

        if (titleMatch || classMatch) {
            return w.handle;
        }
    }

    return 0;
}

} // namespace X11Capture

#endif // PLATFORM_LINUX
