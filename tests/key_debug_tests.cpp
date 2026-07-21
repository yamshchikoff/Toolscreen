#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "platform/linux/hotkey_detect.h"

namespace {

int g_failures = 0;

void CheckStrEq(const std::string& actual, const std::string& expected, const std::string& label) {
    if (actual != expected) {
        std::cerr << "  FAIL: " << label << "\n"
                  << "    expected: \"" << expected << "\"\n"
                  << "    got:      \"" << actual << "\"\n";
        ++g_failures;
    }
}

void ZKeyFormat() {
    CheckStrEq(FormatKeyEvent(90, 52),
               "[Toolscreen] KEY: vk=0x5A (90) scanCode=52",
               "Z (VK 90, sc 52)");
}

void JKeyFormat() {
    CheckStrEq(FormatKeyEvent(74, 44),
               "[Toolscreen] KEY: vk=0x4A (74) scanCode=44",
               "J (VK 74, sc 44)");
}

void AKeyFormat() {
    CheckStrEq(FormatKeyEvent(65, 38),
               "[Toolscreen] KEY: vk=0x41 (65) scanCode=38",
               "A (VK 65, sc 38)");
}

void AltKeyFormat() {
    CheckStrEq(FormatKeyEvent(164, 64),
               "[Toolscreen] KEY: vk=0xA4 (164) scanCode=64",
               "LeftAlt (VK 164, sc 64)");
}

}  // namespace

struct TestCase {
    const char* name;
    std::function<void()> run;
};

const std::vector<TestCase>& Registry() {
    static const std::vector<TestCase> cases = {
        {"z_key_format",   &ZKeyFormat},
        {"j_key_format",   &JKeyFormat},
        {"a_key_format",   &AKeyFormat},
        {"alt_key_format", &AltKeyFormat},
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
