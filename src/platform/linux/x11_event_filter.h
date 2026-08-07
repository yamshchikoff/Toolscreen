#pragma once
#include <cstdint>
#include <X11/Xlib.h>

// Возвращает true, если XEvent является клавиатурным событием
// (KeyPress или KeyRelease).
bool IsKeyEvent(const XEvent& event);

// Возвращает true, если keycode соответствует хоткею LAlt (64).
bool IsAltHotkey(uint32_t keycode);
