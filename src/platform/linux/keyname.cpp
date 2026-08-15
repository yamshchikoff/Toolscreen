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

    // Один символ [a-zA-Z0-9]
    if (name.size() == 1) {
        char c = name[0];
        if (c >= 'a' && c <= 'z') return XK_a + (c - 'a');
        if (c >= 'A' && c <= 'Z') return XK_A + (c - 'A');
        if (c >= '0' && c <= '9') return XK_0 + (c - '0');
    }

    // F1-F12, Escape, и т.п.
    return XStringToKeysym(name.c_str());
}

}  // namespace KeyName
