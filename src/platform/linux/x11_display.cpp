#include "x11_display.h"

#ifdef PLATFORM_LINUX

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include <X11/extensions/Xrandr.h>
#include <X11/XF86keysym.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <atomic>
#include <thread>
#include <unordered_map>

namespace X11Display {

namespace {

Display* g_display = nullptr;
int g_screen = 0;
Window g_root = 0;
Window g_gameWindow = 0;
std::atomic<bool> g_initialized{false};
std::mutex g_mutex;

// Track which thread opened the display (the render thread)
std::atomic<uint64_t> g_renderThreadId{0};

// Previous X11 error handler for chaining
int (*g_oldErrorHandler)(Display*, XErrorEvent*) = nullptr;
int (*g_oldIOErrorHandler)(Display*) = nullptr;
int g_xrandrEventBase = 0;
int g_xrandrErrorBase = 0;
bool g_xrandrAvailable = false;

// Custom error handler (non-fatal errors — log and continue)
int X11ErrorHandler(Display* dpy, XErrorEvent* ev) {
    // BadWindow is expected during window resize/recreate — suppress
    if (ev->error_code == BadWindow) return 0;
    char buffer[256];
    XGetErrorText(dpy, ev->error_code, buffer, sizeof(buffer));
    fprintf(stderr, "[Toolscreen] X11 error: %s (code=%d, opcode=%d, resource=%lu)\n",
            buffer, ev->error_code, ev->request_code, ev->resourceid);
    return 0;
}

// Custom I/O error handler (connection lost — must NOT return per X11 spec)
// Chains to the previously-installed handler so other libraries (LWJGL, etc.)
// get a chance to handle the disconnection before we resort to _exit.
int X11IOErrorHandler(Display* dpy) {
    fprintf(stderr, "[Toolscreen] X11 I/O error: connection to X server lost\n");
    // Chain to previous handler — may longjmp or otherwise recover
    if (g_oldIOErrorHandler && g_oldIOErrorHandler != X11IOErrorHandler) {
        return g_oldIOErrorHandler(dpy);
    }
    _exit(1); // No previous handler — required by X11 spec: I/O error handlers must not return
}

// Map X11 KeySym to canonical Windows VK
const std::unordered_map<KeySym, PlatformVk>& GetKeysymToVkMap() {
    static const std::unordered_map<KeySym, PlatformVk> map = {
        { XK_BackSpace,     Vk::BACK },
        { XK_Tab,           Vk::TAB },
        { XK_Clear,         Vk::CLEAR },
        { XK_Return,        Vk::RETURN },
        { XK_Pause,         Vk::PAUSE },
        { XK_Scroll_Lock,   Vk::SCROLL },
        { XK_Escape,        Vk::ESCAPE },
        { XK_Delete,        Vk::DELETE },
        { XK_Home,          Vk::HOME },
        { XK_Left,          Vk::LEFT },
        { XK_Up,            Vk::UP },
        { XK_Right,         Vk::RIGHT },
        { XK_Down,          Vk::DOWN },
        { XK_Page_Up,       Vk::PRIOR },
        { XK_Page_Down,     Vk::NEXT },
        { XK_End,           Vk::END },
        { XK_Insert,        Vk::INSERT },
        { XK_Print,         Vk::PRINT },
        { XK_KP_Insert,     Vk::INSERT },
        { XK_KP_Delete,     Vk::DELETE },
        { XK_KP_Home,       Vk::HOME },
        { XK_KP_End,        Vk::END },
        { XK_KP_Page_Up,    Vk::PRIOR },
        { XK_KP_Page_Down,  Vk::NEXT },
        { XK_KP_Left,       Vk::LEFT },
        { XK_KP_Right,      Vk::RIGHT },
        { XK_KP_Up,         Vk::UP },
        { XK_KP_Down,       Vk::DOWN },
        { XK_KP_Enter,      Vk::RETURN },
        { XK_KP_Multiply,   Vk::MULTIPLY },
        { XK_KP_Add,        Vk::ADD },
        { XK_KP_Subtract,   Vk::SUBTRACT },
        { XK_KP_Decimal,    Vk::DECIMAL },
        { XK_KP_Divide,     Vk::DIVIDE },
        { XK_KP_0,          Vk::NUMPAD0 },
        { XK_KP_1,          Vk::NUMPAD1 },
        { XK_KP_2,          Vk::NUMPAD2 },
        { XK_KP_3,          Vk::NUMPAD3 },
        { XK_KP_4,          Vk::NUMPAD4 },
        { XK_KP_5,          Vk::NUMPAD5 },
        { XK_KP_6,          Vk::NUMPAD6 },
        { XK_KP_7,          Vk::NUMPAD7 },
        { XK_KP_8,          Vk::NUMPAD8 },
        { XK_KP_9,          Vk::NUMPAD9 },
        { XK_F1,            Vk::F1 },
        { XK_F2,            Vk::F2 },
        { XK_F3,            Vk::F3 },
        { XK_F4,            Vk::F4 },
        { XK_F5,            Vk::F5 },
        { XK_F6,            Vk::F6 },
        { XK_F7,            Vk::F7 },
        { XK_F8,            Vk::F8 },
        { XK_F9,            Vk::F9 },
        { XK_F10,           Vk::F10 },
        { XK_F11,           Vk::F11 },
        { XK_F12,           Vk::F12 },
        { XK_F13,           Vk::F13 },
        { XK_F14,           Vk::F14 },
        { XK_F15,           Vk::F15 },
        { XK_F16,           Vk::F16 },
        { XK_Shift_L,       Vk::LSHIFT },
        { XK_Shift_R,       Vk::RSHIFT },
        { XK_Control_L,     Vk::LCONTROL },
        { XK_Control_R,     Vk::RCONTROL },
        { XK_Alt_L,         Vk::LMENU },
        { XK_Alt_R,         Vk::RMENU },
        { XK_Meta_L,        Vk::LWIN },
        { XK_Meta_R,        Vk::RWIN },
        { XK_Menu,          Vk::APPS },
        { XK_Caps_Lock,     Vk::CAPITAL },
        { XK_Num_Lock,      Vk::NUMLOCK },
        { XK_space,         Vk::SPACE },
        { XK_0,             0x30 },   { XK_1, 0x31 }, { XK_2, 0x32 }, { XK_3, 0x33 }, { XK_4, 0x34 },
        { XK_5,             0x35 },   { XK_6, 0x36 }, { XK_7, 0x37 }, { XK_8, 0x38 }, { XK_9, 0x39 },
        { XK_A,             0x41 },   { XK_B, 0x42 }, { XK_C, 0x43 }, { XK_D, 0x44 }, { XK_E, 0x45 },
        { XK_F,             0x46 },   { XK_G, 0x47 }, { XK_H, 0x48 }, { XK_I, 0x49 }, { XK_J, 0x4A },
        { XK_K,             0x4B },   { XK_L, 0x4C }, { XK_M, 0x4D }, { XK_N, 0x4E }, { XK_O, 0x4F },
        { XK_P,             0x50 },   { XK_Q, 0x51 }, { XK_R, 0x52 }, { XK_S, 0x53 }, { XK_T, 0x54 },
        { XK_U,             0x55 },   { XK_V, 0x56 }, { XK_W, 0x57 }, { XK_X, 0x58 }, { XK_Y, 0x59 },
        { XK_Z,             0x5A },
        { XK_a,             0x41 },   { XK_b, 0x42 }, { XK_c, 0x43 }, { XK_d, 0x44 }, { XK_e, 0x45 },
        { XK_f,             0x46 },   { XK_g, 0x47 }, { XK_h, 0x48 }, { XK_i, 0x49 }, { XK_j, 0x4A },
        { XK_k,             0x4B },   { XK_l, 0x4C }, { XK_m, 0x4D }, { XK_n, 0x4E }, { XK_o, 0x4F },
        { XK_p,             0x50 },   { XK_q, 0x51 }, { XK_r, 0x52 }, { XK_s, 0x53 }, { XK_t, 0x54 },
        { XK_u,             0x55 },   { XK_v, 0x56 }, { XK_w, 0x57 }, { XK_x, 0x58 }, { XK_y, 0x59 },
        { XK_z,             0x5A },
        { XK_semicolon,     Vk::OEM_1 },
        { XK_equal,         Vk::OEM_PLUS },
        { XK_comma,         Vk::OEM_COMMA },
        { XK_minus,         Vk::OEM_MINUS },
        { XK_period,        Vk::OEM_PERIOD },
        { XK_slash,         Vk::OEM_2 },
        { XK_grave,         Vk::OEM_3 },
        { XK_bracketleft,   Vk::OEM_4 },
        { XK_backslash,     Vk::OEM_5 },
        { XK_bracketright,  Vk::OEM_6 },
        { XK_apostrophe,    Vk::OEM_7 },
        { XK_less,          Vk::OEM_102 },
        { XF86XK_AudioMute,        Vk::VOLUME_MUTE },
        { XF86XK_AudioLowerVolume, Vk::VOLUME_DOWN },
        { XF86XK_AudioRaiseVolume, Vk::VOLUME_UP },
        { XF86XK_AudioNext,        Vk::MEDIA_NEXT },
        { XF86XK_AudioPrev,        Vk::MEDIA_PREV },
        { XF86XK_AudioStop,        Vk::MEDIA_STOP },
        { XF86XK_AudioPlay,        Vk::MEDIA_PLAY_PAUSE },
    };
    return map;
}

} // namespace

bool Open() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_display) return true;

    g_display = XOpenDisplay(nullptr);
    if (!g_display) {
        fprintf(stderr, "[Toolscreen] ERROR: Cannot open X11 display\n");
        return false;
    }

    g_screen = DefaultScreen(g_display);
    g_root = RootWindow(g_display, g_screen);
    g_renderThreadId.store(Platform::GetCurrentThreadId());

    // Set error handlers
    g_oldErrorHandler = XSetErrorHandler(X11ErrorHandler);
    g_oldIOErrorHandler = XSetIOErrorHandler(X11IOErrorHandler);

    // Query XRandR for multi-monitor geometry support
    g_xrandrAvailable = XRRQueryExtension(g_display, &g_xrandrEventBase, &g_xrandrErrorBase);

    g_initialized.store(true);
    return true;
}

void Close() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_display) return;

    XSetErrorHandler(g_oldErrorHandler);
    XSetIOErrorHandler(g_oldIOErrorHandler);
    XCloseDisplay(g_display);
    g_display = nullptr;
    g_initialized.store(false);
}

Display* Get() { return g_display; }
int GetScreen() { return g_screen; }
Window GetRoot() { return g_root; }

// Recursively search a window and its children for a Minecraft/LWJGL window
static Window SearchWindowTree(Display* dpy, Window win, Atom netWmName, Atom wmClass, int depth, int& windowsVisited) {
    if (depth > 5 || win == 0 || ++windowsVisited > 1000) return 0;

    // Check this window
    if (netWmName != None) {
        Atom actualType; int actualFormat;
        unsigned long nitems, bytesAfter;
        unsigned char* prop = nullptr;
        if (XGetWindowProperty(dpy, win, netWmName, 0, 1024, False, AnyPropertyType,
                               &actualType, &actualFormat, &nitems, &bytesAfter, &prop) == Success && prop) {
            std::string name(reinterpret_cast<char*>(prop));
            XFree(prop);
            if (name.find("Minecraft") != std::string::npos) return win;
        }
    }
    if (wmClass != None) {
        Atom actualType; int actualFormat;
        unsigned long nitems, bytesAfter;
        unsigned char* prop = nullptr;
        if (XGetWindowProperty(dpy, win, wmClass, 0, 1024, False, AnyPropertyType,
                               &actualType, &actualFormat, &nitems, &bytesAfter, &prop) == Success && prop) {
            std::string klass(reinterpret_cast<char*>(prop));
            XFree(prop);
            if (klass.find("Minecraft") != std::string::npos || klass.find("lwjgl") != std::string::npos) return win;
        }
    }

    // Search children recursively
    Window root, parent, *children = nullptr;
    unsigned int nchildren = 0;
    if (XQueryTree(dpy, win, &root, &parent, &children, &nchildren) && children && nchildren > 0) {
        for (unsigned int i = 0; i < nchildren; ++i) {
            Window result = SearchWindowTree(dpy, children[i], netWmName, wmClass, depth + 1, windowsVisited);
            if (result != 0) { XFree(children); return result; }
        }
        XFree(children);
    }
    return 0;
}

Window FindGameWindow() {
    if (!g_display) return 0;

    Atom netWmName = XInternAtom(g_display, "_NET_WM_NAME", True);
    Atom wmClass = XInternAtom(g_display, "WM_CLASS", True);

    int visited = 0;
    return SearchWindowTree(g_display, RootWindow(g_display, g_screen),
                            netWmName, wmClass, 0, visited);
}

void SetGameWindow(Window win) { g_gameWindow = win; }
Window GetGameWindow() { return g_gameWindow; }

bool GetWindowGeometry(Window win, PlatformRect& outRect) {
    if (!g_display || !win) return false;

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(g_display, win, &attrs)) return false;

    // Convert to root coordinates
    Window child;
    int x, y;
    XTranslateCoordinates(g_display, win, attrs.root, 0, 0, &x, &y, &child);

    outRect.left = x;
    outRect.top = y;
    outRect.right = x + attrs.width;
    outRect.bottom = y + attrs.height;
    return true;
}

std::string GetWindowTitle(Window win) {
    if (!g_display || !win) return "";

    Atom netWmName = XInternAtom(g_display, "_NET_WM_NAME", True);
    Atom actualType;
    int actualFormat;
    unsigned long nitems, bytesAfter;
    unsigned char* prop = nullptr;

    if (XGetWindowProperty(g_display, win, netWmName, 0, 1024, False, AnyPropertyType,
                           &actualType, &actualFormat, &nitems, &bytesAfter, &prop) == Success && prop) {
        std::string name(reinterpret_cast<char*>(prop));
        XFree(prop);
        return name;
    }

    // Fallback: XFetchName
    char* name = nullptr;
    if (XFetchName(g_display, win, &name) && name) {
        std::string result(name);
        XFree(name);
        return result;
    }
    return "";
}

GLXContext GetCurrentContext() { return glXGetCurrentContext(); }
GLXDrawable GetCurrentDrawable() { return glXGetCurrentDrawable(); }

Display* GetCurrentDisplay() { return glXGetCurrentDisplay(); }

bool IsOnRenderThread() {
    return Platform::GetCurrentThreadId() == g_renderThreadId.load();
}

void Flush() {
    if (g_display) XFlush(g_display);
}

int GetMonitorCount() {
    if (!g_display) return 1;

    if (g_xrandrAvailable) {
        XRRScreenResources* res = XRRGetScreenResources(g_display, g_root);
        if (res) {
            int count = res->ncrtc;
            XRRFreeScreenResources(res);
            return count > 0 ? count : 1;
        }
    }
    return ScreenCount(g_display);
}

bool GetMonitorGeometry(int index, PlatformRect& outRect, bool& outIsPrimary) {
    if (!g_display) return false;

    if (g_xrandrAvailable) {
        XRRScreenResources* res = XRRGetScreenResources(g_display, g_root);
        if (res) {
            if (index >= 0 && index < res->ncrtc) {
                XRRCrtcInfo* crtc = XRRGetCrtcInfo(g_display, res, res->crtcs[index]);
                if (crtc) {
                    outRect.left   = static_cast<s32>(crtc->x);
                    outRect.top    = static_cast<s32>(crtc->y);
                    outRect.right  = static_cast<s32>(crtc->x + crtc->width);
                    outRect.bottom = static_cast<s32>(crtc->y + crtc->height);

                    // Determine if this is the primary output
                    RROutput primaryOut = XRRGetOutputPrimary(g_display, g_root);
                    outIsPrimary = false;
                    for (int o = 0; o < crtc->noutput; ++o) {
                        if (crtc->outputs[o] == primaryOut) {
                            outIsPrimary = true;
                            break;
                        }
                    }

                    XRRFreeCrtcInfo(crtc);
                    XRRFreeScreenResources(res);
                    return true;
                }
            }
            XRRFreeScreenResources(res);
        }
    }

    // Fallback: X11 Screen (all monitors in one desktop)
    int screenCount = ScreenCount(g_display);
    if (index < 0 || index >= screenCount) return false;

    Screen* screen = ScreenOfDisplay(g_display, index);
    if (!screen) return false;

    outRect.left = 0;
    outRect.top = 0;
    outRect.right = WidthOfScreen(screen);
    outRect.bottom = HeightOfScreen(screen);
    outIsPrimary = (index == 0);
    return true;
}

uint32_t X11KeyToVk(KeySym keysym, unsigned int /*keycode*/, unsigned int state) {
    // TODO: use `state` to handle CapsLock/NumLock modifiers for correct
    // key identification when those locks are active.
    // Direct keysym mapping
    auto& map = GetKeysymToVkMap();
    auto it = map.find(keysym);
    if (it != map.end()) return it->second;

    // Handle latin-1 printable range (0x20-0x7E)
    if (keysym >= 0x20 && keysym <= 0x7E) {
        // Check if this is a letter key
        if ((keysym >= XK_A && keysym <= XK_Z) || (keysym >= XK_a && keysym <= XK_z)) {
            return (uint32_t)std::toupper((char)keysym);
        }
        // Digits and symbols map directly
        return (uint32_t)keysym;
    }

    // Cyrillic range (0x400-0x4FF) — shouldn't reach here with XKeycodeToVk
    // using group=0 level=0, but if it does, log and fall through to 0
    if (keysym >= 0x400 && keysym <= 0x4FF) {
        fprintf(stderr, "[Toolscreen] WARNING: Cyrillic keysym 0x%lX reached X11KeyToVk — "
                "XKeycodeToVk should have mapped to Latin equivalent\n",
                static_cast<unsigned long>(keysym));
        return 0;
    }

    return 0;
}

KeySym VkToX11Keysym(PlatformVk vk) {
    // Reverse lookup
    auto& map = GetKeysymToVkMap();
    for (auto& [ks, v] : map) {
        if (v == vk) return ks;
    }
    // Letters and digits
    if (vk >= 0x30 && vk <= 0x39) return (KeySym)(XK_0 + (vk - 0x30));
    if (vk >= 0x41 && vk <= 0x5A) return (KeySym)(XK_A + (vk - 0x41));
    return NoSymbol;
}

std::string KeysymToString(KeySym keysym) {
    if (!g_display) return "";
    char* str = XKeysymToString(keysym);
    if (str) return std::string(str);

    // Handle standard printable range
    if (keysym >= 0x20 && keysym <= 0x7E) {
        return std::string(1, (char)keysym);
    }
    return "";
}

} // namespace X11Display

#endif // PLATFORM_LINUX
