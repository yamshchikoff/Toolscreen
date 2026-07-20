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

// Fast XImage → RGBA8 conversion without XGetPixel.
// Works directly with image->data, handling 24/32 bpp (BGRX/BGRA) and 16 bpp.
void CopyXImageToRGBA(const XImage* image, int w, int h,
                      std::vector<uint8_t>& outRgba) {
    const int bpp = image->bits_per_pixel;
    const int bytesPerPixel = bpp / 8;
    const uint8_t* src = reinterpret_cast<const uint8_t*>(image->data);

    if (bpp >= 24) {
        // 24 or 32 bpp: direct BGR/BGRA → RGBA copy
        for (int row = 0; row < h; ++row) {
            for (int col = 0; col < w; ++col) {
                size_t srcIdx = static_cast<size_t>(row) * image->bytes_per_line
                              + static_cast<size_t>(col) * bytesPerPixel;
                size_t dstIdx = (static_cast<size_t>(row) * w + col) * 4;
                outRgba[dstIdx + 0] = src[srcIdx + 2]; // R (from BGR)
                outRgba[dstIdx + 1] = src[srcIdx + 1]; // G
                outRgba[dstIdx + 2] = src[srcIdx + 0]; // B
                outRgba[dstIdx + 3] = 255;             // A
            }
        }
    } else if (bpp >= 15) {
        // 16 or 15 bpp: unpack using image masks with 5→8 / 6→8 bit scaling
        const unsigned long rm = image->red_mask;
        const unsigned long gm = image->green_mask;
        const unsigned long bm = image->blue_mask;

        // Precompute shifts (count trailing zeros)
        const int rshift = __builtin_ctzl(rm);
        const int gshift = __builtin_ctzl(gm);
        const int bshift = __builtin_ctzl(bm);

        // Bit counts per channel (e.g., 5 for RGB555, 5/6/5 for RGB565)
        const int rbits = __builtin_popcountl(rm);
        const int gbits = __builtin_popcountl(gm);
        const int bbits = __builtin_popcountl(bm);

        // Scale N-bit value to 8-bit: v * 255 / (2^bits - 1)
        auto scaleTo8 = [](unsigned long v, int bits) -> uint8_t {
            if (bits >= 8) return static_cast<uint8_t>(v);
            unsigned int maxVal = (1u << bits) - 1;
            return static_cast<uint8_t>((v * 255u + maxVal / 2) / maxVal); // rounded
        };

        for (int row = 0; row < h; ++row) {
            for (int col = 0; col < w; ++col) {
                size_t srcIdx = static_cast<size_t>(row) * image->bytes_per_line
                              + static_cast<size_t>(col) * bytesPerPixel;
                size_t dstIdx = (static_cast<size_t>(row) * w + col) * 4;

                uint16_t pixel;
                memcpy(&pixel, src + srcIdx, sizeof(pixel));

                outRgba[dstIdx + 0] = scaleTo8((pixel & rm) >> rshift, rbits);
                outRgba[dstIdx + 1] = scaleTo8((pixel & gm) >> gshift, gbits);
                outRgba[dstIdx + 2] = scaleTo8((pixel & bm) >> bshift, bbits);
                outRgba[dstIdx + 3] = 255;
            }
        }
    } else {
        // 8 bpp or lower: rare, skip (fill black)
        memset(outRgba.data(), 0, outRgba.size());
    }
}

} // namespace

bool CaptureScreen(int x, int y, int w, int h,
                   std::vector<uint8_t>& outRgba,
                   int& outWidth, int& outHeight) {
    Display* dpy = X11Display::Get();
    if (!dpy) return false;

    // Thread-safe one-shot SHM init (fixes race on shmTried)
    static std::once_flag shmInitFlag;
    std::call_once(shmInitFlag, InitShm);

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
    outRgba.resize(static_cast<size_t>(w) * h * 4);

    if (g_shmAvailable) {
        // XShm path — fast shared memory transfer
        XShmSegmentInfo shmInfo;
        XImage* image = XShmCreateImage(dpy, DefaultVisual(dpy, X11Display::GetScreen()),
                                         DefaultDepth(dpy, X11Display::GetScreen()),
                                         ZPixmap, nullptr, &shmInfo, w, h);
        if (!image) goto fallback;

        shmInfo.shmid = shmget(IPC_PRIVATE, static_cast<size_t>(image->bytes_per_line) * image->height,
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

        CopyXImageToRGBA(image, w, h, outRgba);

        XShmDetach(dpy, &shmInfo);
        XDestroyImage(image);
        shmdt(shmInfo.shmaddr);
        return true;
    }

fallback:
    // XGetImage fallback — slower but always works
    XImage* image = XGetImage(dpy, root, x, y, w, h, AllPlanes, ZPixmap);
    if (!image) return false;

    CopyXImageToRGBA(image, w, h, outRgba);

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

    outRgba.resize(static_cast<size_t>(outWidth) * outHeight * 4);

    CopyXImageToRGBA(image, outWidth, outHeight, outRgba);

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
