#include "x11_event_filter.h"

bool IsKeyEvent(const XEvent& event) {
    return event.type == KeyPress || event.type == KeyRelease;
}
