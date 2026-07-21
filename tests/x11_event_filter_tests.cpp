#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <X11/Xlib.h>

#include "platform/linux/x11_event_filter.h"

namespace {

int g_failures = 0;

void CheckBool(bool actual, bool expected, const std::string& label) {
    if (actual != expected) {
        std::cerr << "  FAIL: " << label << " expected " << (expected ? "true" : "false")
                  << " got " << (actual ? "true" : "false") << "\n";
        ++g_failures;
    }
}

void KeyPressIsKeyEvent() {
    XEvent ev{};
    ev.type = KeyPress;
    CheckBool(IsKeyEvent(ev), true, "KeyPress");
}

void KeyReleaseIsKeyEvent() {
    XEvent ev{};
    ev.type = KeyRelease;
    CheckBool(IsKeyEvent(ev), true, "KeyRelease");
}

void MotionNotifyIsNotKeyEvent() {
    XEvent ev{};
    ev.type = MotionNotify;
    CheckBool(IsKeyEvent(ev), false, "MotionNotify");
}

void ButtonPressIsNotKeyEvent() {
    XEvent ev{};
    ev.type = ButtonPress;
    CheckBool(IsKeyEvent(ev), false, "ButtonPress");
}

}  // namespace

struct TestCase {
    const char* name;
    std::function<void()> run;
};

const std::vector<TestCase>& Registry() {
    static const std::vector<TestCase> cases = {
        {"keypress_is_key_event",   &KeyPressIsKeyEvent},
        {"keyrelease_is_key_event", &KeyReleaseIsKeyEvent},
        {"motion_not_key_event",    &MotionNotifyIsNotKeyEvent},
        {"button_not_key_event",    &ButtonPressIsNotKeyEvent},
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
