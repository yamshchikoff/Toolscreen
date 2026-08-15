#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "platform/linux/keyname.h"

namespace {

int g_failures = 0;

void CheckKeysym(const std::string& name, KeySym expected) {
    KeySym actual = KeyName::ToKeysym(name);
    if (actual != expected) {
        std::cerr << "  FAIL: KeyNameToKeysym(\"" << name << "\") expected 0x"
                  << std::hex << expected << " got 0x" << actual << std::dec << "\n";
        ++g_failures;
    }
}

void LetterJ() { CheckKeysym("J", XK_J); }
void LetterZ() { CheckKeysym("Z", XK_Z); }
void Lowercase() { CheckKeysym("j", XK_j); }
void LeftAlt() { CheckKeysym("LAlt", XK_Alt_L); }
void RightShift() { CheckKeysym("RShift", XK_Shift_R); }
void F11() { CheckKeysym("F11", XK_F11); }
void Space() { CheckKeysym("Space", XK_space); }
void Digit() { CheckKeysym("5", XK_5); }
void Apostrophe() { CheckKeysym("'", XK_apostrophe); }
void Period() { CheckKeysym(".", XK_period); }
void Slash() { CheckKeysym("/", XK_slash); }
void Dash() { CheckKeysym("-", XK_minus); }
void Unknown() { CheckKeysym("NoSuchKey", NoSymbol); }

}  // namespace

struct TestCase {
    const char* name;
    std::function<void()> run;
};

const std::vector<TestCase>& Registry() {
    static const std::vector<TestCase> cases = {
        {"letter_j", LetterJ},
        {"letter_z", LetterZ},
        {"lowercase_j", Lowercase},
        {"left_alt", LeftAlt},
        {"right_shift", RightShift},
        {"f11", F11},
        {"space", Space},
        {"digit_5", Digit},
        {"apostrophe", Apostrophe},
        {"period", Period},
        {"slash", Slash},
        {"dash", Dash},
        {"unknown", Unknown},
    };
    return cases;
}

int RunNamed(const std::string& name) {
    for (const auto& testCase : Registry()) {
        if (name == testCase.name) {
            g_failures = 0;
            std::cout << "RUN " << name << '\n';
            testCase.run();
            if (g_failures == 0) {
                std::cout << "PASS " << name << '\n';
                return 0;
            }
            std::cerr << "FAIL " << name << " (" << g_failures << " assertion(s))\n";
            return 1;
        }
    }
    std::cerr << "Unknown test case: " << name << '\n';
    return 2;
}

int RunAll() {
    int failed = 0;
    for (const auto& testCase : Registry()) {
        if (RunNamed(testCase.name) != 0) ++failed;
    }
    return failed == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
    if (argc == 1 || (argc == 2 && std::strcmp(argv[1], "--run-all") == 0)) {
        return RunAll();
    }
    if (argc == 2 && std::strcmp(argv[1], "--list") == 0) {
        for (const auto& testCase : Registry()) std::cout << testCase.name << '\n';
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--run") == 0) {
        return RunNamed(argv[2]);
    }
    std::cerr << "Usage: " << argv[0] << " [--run <case> | --run-all | --list]\n";
    return 2;
}
