#pragma once

#include <cstdint>

namespace ResizeConfig {

struct HotkeyBinding {
    uint32_t keycode;
    int width;        // целевая ширина режима
    int height;       // целевая высота режима
    int originalW;    // исходная ширина (Fullscreen)
    int originalH;    // исходная высота (Fullscreen)
    bool active;      // сейчас в secondaryMode? (toggle)
};

// Загрузить ~/.toolscreen/resize_bindings.json.
// Вычислить пиксельные размеры на основе screenW/screenH.
bool Load(int screenW, int screenH);

// Найти бинд по X11 keycode. Возвращает nullptr если не найден.
HotkeyBinding* Find(uint32_t keycode);

}  // namespace ResizeConfig
