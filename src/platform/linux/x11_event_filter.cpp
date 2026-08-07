#include "x11_event_filter.h"

bool IsKeyEvent(const XEvent& event) {
    return event.type == KeyPress || event.type == KeyRelease;
}

bool IsAltHotkey(uint32_t keycode) {
    return keycode == 64;  // X11 keycode для Left Alt
}
