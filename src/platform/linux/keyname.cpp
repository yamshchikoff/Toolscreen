#include "keyname.h"

#include <X11/keysym.h>
#include <unordered_map>

namespace KeyName {

KeySym ToKeysym(const std::string& name) {
    static const std::unordered_map<std::string, KeySym> aliases = {
        {"LAlt",   XK_Alt_L},
        {"RAlt",   XK_Alt_R},
        {"LShift", XK_Shift_L},
        {"RShift", XK_Shift_R},
        {"LCtrl",  XK_Control_L},
        {"RCtrl",  XK_Control_R},
        {"LWin",   XK_Super_L},
        {"RWin",   XK_Super_R},
        {"Enter",  XK_Return},
        {"Esc",    XK_Escape},
        {"Space",  XK_space},
    };
    auto it = aliases.find(name);
    if (it != aliases.end()) return it->second;

    // Один печатный ASCII-символ (0x20–0x7E) → его keysym совпадает с кодом
    if (name.size() == 1) {
        unsigned char c = (unsigned char)name[0];
        if (c >= 0x20 && c <= 0x7E) return (KeySym)c;
    }

    // F1-F12, Escape, и т.п.
    return XStringToKeysym(name.c_str());
}

}  // namespace KeyName
