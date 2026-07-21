#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "platform/linux/hotkey_detect.h"

namespace {

int g_failures = 0;

void CheckBool(bool actual, bool expected, const std::string& label) {
    if (actual != expected) {
        std::cerr << "  FAIL: " << label << " expected " << (expected ? "true" : "false")
                  << " got " << (actual ? "true" : "false") << "\n";
        ++g_failures;
    }
}

void ZKeyIsHotkey() {
    CheckBool(IsHotkeyVkCode(90), true, "Z (VK 90)");
}

void JKeyIsHotkey() {
    CheckBool(IsHotkeyVkCode(74), true, "J (VK 74)");
}

void LeftAltIsHotkey() {
    CheckBool(IsHotkeyVkCode(164), true, "LeftAlt (VK 164)");
}

void AKeyIsNotHotkey() {
    CheckBool(IsHotkeyVkCode(65), false, "A (VK 65) — не хоткей");
}

void ZeroIsNotHotkey() {
    CheckBool(IsHotkeyVkCode(0), false, "0 — невалидный VK");
}

}  // namespace

struct TestCase {
    const char* name;
    std::function<void()> run;
};

const std::vector<TestCase>& Registry() {
    static const std::vector<TestCase> cases = {
        {"z_is_hotkey",   &ZKeyIsHotkey},
        {"j_is_hotkey",   &JKeyIsHotkey},
        {"leftalt_is_hotkey", &LeftAltIsHotkey},
        {"a_is_not_hotkey", &AKeyIsNotHotkey},
        {"zero_is_not_hotkey", &ZeroIsNotHotkey},
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
