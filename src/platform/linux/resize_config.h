#pragma once

#include <cstdint>
#include <string>

namespace ResizeConfig {

struct HotkeyBinding {
    uint32_t keycode;
    int width;        // ширина окна (capped)
    int height;       // высота окна (capped)
    int viewportW;    // ширина viewport'а (без капа)
    int viewportH;    // высота viewport'а (без капа)
    std::string mode; // имя режима (Thin/EyeZoom/Wide)
};

// Загрузить ~/.toolscreen/resize_bindings.json.
// Вычислить пиксельные размеры на основе screenW/screenH.
bool Load(int screenW, int screenH);

// Найти бинд по X11 keycode. Возвращает nullptr если не найден.
const HotkeyBinding* Find(uint32_t keycode);

// Размеры viewport'а для текущего активного режима.
// (0,0) если активного режима нет (Fullscreen).
void GetViewportForActiveMode(int& outW, int& outH);

// Текущий активный режим ("" = Fullscreen, иначе Thin/EyeZoom/Wide)
const char* GetActiveMode();
void SetActiveMode(const char* mode);

// Исходный размер окна (сохраняется при первом ресайзе)
void SetOriginalSize(int w, int h);
int GetOriginalW();
int GetOriginalH();

}  // namespace ResizeConfig
