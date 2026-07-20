#include "imgui_impl_x11.h"

#ifdef PLATFORM_LINUX

#include "imgui.h"
// imgui.h #undefs X11 Status macro — restore as typedef for X11 headers below
typedef int Status;
#include "platform/linux/x11_display.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <atomic>

namespace {

Display* g_display = nullptr;
Window g_window = 0;
struct timespec g_startTime;

// Mouse state (tracked from events, not polled from X11)
int g_mouseX = 0, g_mouseY = 0;
std::atomic<bool> g_mouseButtons[5] = {};
float g_mouseWheel = 0.0f;

// Modifier state tracked from X11 key events (not LED indicators)
bool g_keyCtrl  = false;
bool g_keyShift = false;
bool g_keyAlt   = false;
bool g_keySuper = false;

// Convert X11 KeySym to ImGuiKey (v1.92.6+ ImGuiKey enum)
ImGuiKey X11KeySymToImGuiKey(KeySym keysym) {
    switch (keysym) {
        case XK_Tab:        return ImGuiKey_Tab;
        case XK_Left:       return ImGuiKey_LeftArrow;
        case XK_Right:      return ImGuiKey_RightArrow;
        case XK_Up:         return ImGuiKey_UpArrow;
        case XK_Down:       return ImGuiKey_DownArrow;
        case XK_Page_Up:    return ImGuiKey_PageUp;
        case XK_Page_Down:  return ImGuiKey_PageDown;
        case XK_Home:       return ImGuiKey_Home;
        case XK_End:        return ImGuiKey_End;
        case XK_Insert:     return ImGuiKey_Insert;
        case XK_Delete:     return ImGuiKey_Delete;
        case XK_BackSpace:  return ImGuiKey_Backspace;
        case XK_space:      return ImGuiKey_Space;
        case XK_Return:     return ImGuiKey_Enter;
        case XK_Escape:     return ImGuiKey_Escape;
        case XK_Control_L:  return ImGuiKey_LeftCtrl;
        case XK_Control_R:  return ImGuiKey_RightCtrl;
        case XK_Shift_L:    return ImGuiKey_LeftShift;
        case XK_Shift_R:    return ImGuiKey_RightShift;
        case XK_Alt_L:      return ImGuiKey_LeftAlt;
        case XK_Alt_R:      return ImGuiKey_RightAlt;
        case XK_Super_L:    return ImGuiKey_LeftSuper;
        case XK_Super_R:    return ImGuiKey_RightSuper;
        case XK_Menu:       return ImGuiKey_Menu;
        case XK_0:          return ImGuiKey_0;
        case XK_1:          return ImGuiKey_1;
        case XK_2:          return ImGuiKey_2;
        case XK_3:          return ImGuiKey_3;
        case XK_4:          return ImGuiKey_4;
        case XK_5:          return ImGuiKey_5;
        case XK_6:          return ImGuiKey_6;
        case XK_7:          return ImGuiKey_7;
        case XK_8:          return ImGuiKey_8;
        case XK_9:          return ImGuiKey_9;
        case XK_A: case XK_a: return ImGuiKey_A;
        case XK_B: case XK_b: return ImGuiKey_B;
        case XK_C: case XK_c: return ImGuiKey_C;
        case XK_D: case XK_d: return ImGuiKey_D;
        case XK_E: case XK_e: return ImGuiKey_E;
        case XK_F: case XK_f: return ImGuiKey_F;
        case XK_G: case XK_g: return ImGuiKey_G;
        case XK_H: case XK_h: return ImGuiKey_H;
        case XK_I: case XK_i: return ImGuiKey_I;
        case XK_J: case XK_j: return ImGuiKey_J;
        case XK_K: case XK_k: return ImGuiKey_K;
        case XK_L: case XK_l: return ImGuiKey_L;
        case XK_M: case XK_m: return ImGuiKey_M;
        case XK_N: case XK_n: return ImGuiKey_N;
        case XK_O: case XK_o: return ImGuiKey_O;
        case XK_P: case XK_p: return ImGuiKey_P;
        case XK_Q: case XK_q: return ImGuiKey_Q;
        case XK_R: case XK_r: return ImGuiKey_R;
        case XK_S: case XK_s: return ImGuiKey_S;
        case XK_T: case XK_t: return ImGuiKey_T;
        case XK_U: case XK_u: return ImGuiKey_U;
        case XK_V: case XK_v: return ImGuiKey_V;
        case XK_W: case XK_w: return ImGuiKey_W;
        case XK_X: case XK_x: return ImGuiKey_X;
        case XK_Y: case XK_y: return ImGuiKey_Y;
        case XK_Z: case XK_z: return ImGuiKey_Z;
        case XK_F1:         return ImGuiKey_F1;
        case XK_F2:         return ImGuiKey_F2;
        case XK_F3:         return ImGuiKey_F3;
        case XK_F4:         return ImGuiKey_F4;
        case XK_F5:         return ImGuiKey_F5;
        case XK_F6:         return ImGuiKey_F6;
        case XK_F7:         return ImGuiKey_F7;
        case XK_F8:         return ImGuiKey_F8;
        case XK_F9:         return ImGuiKey_F9;
        case XK_F10:        return ImGuiKey_F10;
        case XK_F11:        return ImGuiKey_F11;
        case XK_F12:        return ImGuiKey_F12;
        case XK_apostrophe: return ImGuiKey_Apostrophe;
        case XK_comma:      return ImGuiKey_Comma;
        case XK_minus:      return ImGuiKey_Minus;
        case XK_period:     return ImGuiKey_Period;
        case XK_slash:      return ImGuiKey_Slash;
        case XK_semicolon:  return ImGuiKey_Semicolon;
        case XK_equal:      return ImGuiKey_Equal;
        case XK_bracketleft:  return ImGuiKey_LeftBracket;
        case XK_backslash:    return ImGuiKey_Backslash;
        case XK_bracketright: return ImGuiKey_RightBracket;
        case XK_grave:        return ImGuiKey_GraveAccent;
        case XK_Caps_Lock:    return ImGuiKey_CapsLock;
        case XK_Scroll_Lock:  return ImGuiKey_ScrollLock;
        case XK_Num_Lock:     return ImGuiKey_NumLock;
        case XK_Print:        return ImGuiKey_PrintScreen;
        case XK_Pause:        return ImGuiKey_Pause;
        case XK_KP_0:       return ImGuiKey_Keypad0;
        case XK_KP_1:       return ImGuiKey_Keypad1;
        case XK_KP_2:       return ImGuiKey_Keypad2;
        case XK_KP_3:       return ImGuiKey_Keypad3;
        case XK_KP_4:       return ImGuiKey_Keypad4;
        case XK_KP_5:       return ImGuiKey_Keypad5;
        case XK_KP_6:       return ImGuiKey_Keypad6;
        case XK_KP_7:       return ImGuiKey_Keypad7;
        case XK_KP_8:       return ImGuiKey_Keypad8;
        case XK_KP_9:       return ImGuiKey_Keypad9;
        case XK_KP_Decimal:  return ImGuiKey_KeypadDecimal;
        case XK_KP_Divide:   return ImGuiKey_KeypadDivide;
        case XK_KP_Multiply: return ImGuiKey_KeypadMultiply;
        case XK_KP_Subtract: return ImGuiKey_KeypadSubtract;
        case XK_KP_Add:      return ImGuiKey_KeypadAdd;
        case XK_KP_Enter:    return ImGuiKey_KeypadEnter;
        default:             return ImGuiKey_None;
    }
}

double GetTimeSinceStart() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - g_startTime.tv_sec) +
           (now.tv_nsec - g_startTime.tv_nsec) / 1000000000.0;
}

} // namespace

bool ImGui_ImplX11_Init(Display* display, Window window) {
    g_display = display;
    g_window = window;
    clock_gettime(CLOCK_MONOTONIC, &g_startTime);

    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = "imgui_impl_x11";
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    fprintf(stderr, "[Toolscreen] ImGui X11 backend initialized (API v1.92+)\n");
    return true;
}

void ImGui_ImplX11_Shutdown() {
    g_display = nullptr;
    g_window = 0;
}

void ImGui_ImplX11_NewFrame() {
    ImGuiIO& io = ImGui::GetIO();

    // Update time
    double now = GetTimeSinceStart();
    static double s_lastTime = now;
    io.DeltaTime = static_cast<float>(now - s_lastTime);
    s_lastTime = now;

    // Update mouse position from X11 (v1.92+ API)
    if (g_display && g_window) {
        Window root, child;
        int rootX, rootY, winX, winY;
        unsigned int mask;
        if (XQueryPointer(g_display, g_window, &root, &child,
                         &rootX, &rootY, &winX, &winY, &mask)) {
            g_mouseX = winX;
            g_mouseY = winY;
        }
    }

    io.AddMousePosEvent(static_cast<float>(g_mouseX), static_cast<float>(g_mouseY));

    // Flush pending mouse button events (set by HandleMouseButtonEvent)
    for (int i = 0; i < 5; ++i) {
        io.AddMouseButtonEvent(i, g_mouseButtons[i]);
    }

    // Flush pending mouse wheel
    if (g_mouseWheel != 0.0f) {
        io.AddMouseWheelEvent(0.0f, g_mouseWheel);
        g_mouseWheel = 0.0f;
    }

    // Modifier state (tracked from key events, not LED indicators)
    io.AddKeyEvent(ImGuiKey_LeftCtrl,  g_keyCtrl);
    io.AddKeyEvent(ImGuiKey_RightCtrl, g_keyCtrl);
    io.AddKeyEvent(ImGuiKey_LeftShift, g_keyShift);
    io.AddKeyEvent(ImGuiKey_RightShift,g_keyShift);
    io.AddKeyEvent(ImGuiKey_LeftAlt,   g_keyAlt);
    io.AddKeyEvent(ImGuiKey_RightAlt,  g_keyAlt);
    io.AddKeyEvent(ImGuiKey_LeftSuper, g_keySuper);
    io.AddKeyEvent(ImGuiKey_RightSuper,g_keySuper);
}

bool ImGui_ImplX11_HandleKeyEvent(unsigned int keycode, bool isDown, unsigned int state) {
    if (!g_display) return false;

    // Track modifier state from key events.
    // Always query level 0 — modifier keysyms (Shift_L, Alt_R, etc.) are
    // independent of Shift state, and Shift+Alt_R may produce XK_Mode_switch
    // on some layouts (e.g. German), breaking modifier tracking.
    KeySym keysym = XkbKeycodeToKeysym(g_display, keycode, 0, 0);
    switch (keysym) {
        case XK_Control_L: case XK_Control_R: g_keyCtrl  = isDown; break;
        case XK_Shift_L:   case XK_Shift_R:   g_keyShift = isDown; break;
        case XK_Alt_L:     case XK_Alt_R:     g_keyAlt   = isDown; break;
        case XK_Super_L:   case XK_Super_R:   g_keySuper = isDown; break;
        default: break;
    }

    ImGuiKey imKey = X11KeySymToImGuiKey(keysym);
    if (imKey != ImGuiKey_None) {
        ImGui::GetIO().AddKeyEvent(imKey, isDown);
    }

    return ImGui::GetIO().WantCaptureKeyboard;
}

bool ImGui_ImplX11_HandleCharEvent(unsigned int charCode) {
    if (!g_display) return false;

    ImGuiIO& io = ImGui::GetIO();
    if (charCode > 0 && charCode < 0x10000) {
        io.AddInputCharacter(static_cast<ImWchar>(charCode));
    }

    return io.WantCaptureKeyboard;
}

bool ImGui_ImplX11_HandleMouseButtonEvent(int button, bool isDown) {
    if (button >= 0 && button < 5) {
        g_mouseButtons[button] = isDown;
    }
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGui_ImplX11_HandleMouseMotionEvent(int x, int y) {
    g_mouseX = x;
    g_mouseY = y;
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGui_ImplX11_HandleMouseWheelEvent(int delta) {
    g_mouseWheel += delta / 120.0f;
    return ImGui::GetIO().WantCaptureMouse;
}

#endif // PLATFORM_LINUX
