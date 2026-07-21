#pragma once
#include <cstdint>
#include <string>

// Возвращает true если VK-код соответствует клавише хоткея
// (Z=90, J=74, LeftAlt=164 — дефолтные хоткеи смены режима).
bool IsHotkeyVkCode(uint32_t vkCode);

// Форматирует отладочную строку для логирования нажатия клавиши.
std::string FormatKeyEvent(uint32_t vkCode, uint32_t scanCode);
