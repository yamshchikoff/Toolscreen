#pragma once

#include <X11/Xlib.h>
#include <string>

namespace KeyName {

// Сопоставить имя кнопки (напр. "J", "LAlt", "F11") с X11 KeySym.
// NoSymbol если не распознано. Не требует Display-соединения.
KeySym ToKeysym(const std::string& name);

}  // namespace KeyName
