#include "hotkey_detect.h"

bool IsHotkeyVkCode(uint32_t vkCode) {
    // Дефолтные хоткеи смены режима: Z(90), J(74), LeftAlt(164)
    switch (vkCode) {
        case 90:  // Vk::KEY_Z → Thin
        case 74:  // Vk::KEY_J → EyeZoom
        case 164: // Vk::LMENU → Wide
            return true;
        default:
            return false;
    }
}
