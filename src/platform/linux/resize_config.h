#pragma once

#include <cstdint>

namespace ResizeConfig {

struct HotkeyBinding {
    uint32_t keycode;
    int width;
    int height;
};

// Загрузить ~/.toolscreen/resize_bindings.json.
// Вычислить пиксельные размеры на основе screenW/screenH.
bool Load(int screenW, int screenH);

// Найти бинд по X11 keycode. Возвращает nullptr если не найден.
const HotkeyBinding* Find(uint32_t keycode);

}  // namespace ResizeConfig
