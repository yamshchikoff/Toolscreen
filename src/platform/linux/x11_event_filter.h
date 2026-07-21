#pragma once
#include <X11/Xlib.h>

// Возвращает true, если XEvent является клавиатурным событием
// (KeyPress или KeyRelease).
bool IsKeyEvent(const XEvent& event);
