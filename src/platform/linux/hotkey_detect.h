#pragma once
#include <cstdint>

// Возвращает true если VK-код соответствует клавише хоткея
// (Z=90, J=74, LeftAlt=164 — дефолтные хоткеи смены режима).
bool IsHotkeyVkCode(uint32_t vkCode);
