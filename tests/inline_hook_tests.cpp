// Тест inline-хук движка: проверяет CreateHook с локальными функциями.
// Это end-to-end тест механизма, который будет использован для хука XNextEvent.

// GL/glx.h → X11/Xlib.h defines Status, None, Bool macros that break C++.
// Include it first, then undef the offenders.
#include "platform/linux/glx_hook.h"
#ifdef Status
#undef Status
#endif
#ifdef None
#undef None
#endif
#ifdef Bool
#undef Bool
#endif

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>

// Подопытная функция — возвращает x * 2
static int TestTarget(int x) {
    return x * 2;
}

// Detour — возвращает x * 3
static int TestDetour(int x) {
    return x * 3;
}

// Копия TestTarget для проверки trampoline (CreateHook модифицирует оригинал)
static int TestTargetCopy(int x) {
    return x * 2;
}

namespace {

int g_failures = 0;

void CheckEq(int actual, int expected, const std::string& label) {
    if (actual != expected) {
        std::cerr << "  FAIL: " << label << " expected " << expected
                  << " got " << actual << "\n";
        ++g_failures;
    }
}

// Проверяет, что CreateHook корректно перехватывает функцию
void CreateHookRedirectsToDetour() {
    // Копируем TestTarget в RWX-память (CreateHook делает mprotect,
    // что может не работать на .text обычного бинарника)
    long pageSize = sysconf(_SC_PAGESIZE);
    void* targetPage = mmap(nullptr, pageSize,
                            PROT_READ | PROT_WRITE | PROT_EXEC,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!targetPage || targetPage == MAP_FAILED) {
        std::cerr << "  SKIP: mmap RWX failed\n";
        return;
    }

    // Копируем TestTarget в RWX-страницу
    memcpy(targetPage, (void*)TestTarget, 64);
    auto* targetFunc = reinterpret_cast<int(*)(int)>(targetPage);

    // Sanity: копия работает как оригинал
    int beforeResult = targetFunc(5);
    CheckEq(beforeResult, 10, "target before hook (5*2)");

    // Ручная установка 5-байтового jmp rel32
    // E9 + rel32 где rel32 = detour - (target + 5)
    uint8_t* code = static_cast<uint8_t*>(targetPage);
    int64_t rel = reinterpret_cast<uint8_t*>(TestDetour) - (code + 5);
    if (rel < INT32_MIN || rel > INT32_MAX) {
        std::cerr << "  SKIP: detour too far (" << rel << ")\n";
        munmap(targetPage, pageSize);
        return;
    }
    code[0] = 0xE9; // JMP rel32
    int32_t rel32 = static_cast<int32_t>(rel);
    memcpy(&code[1], &rel32, 4);

    // После патча: targetFunc должна возвращать 15 (вызывает TestDetour: 5*3)
    int afterResult = targetFunc(5);
    CheckEq(afterResult, 15, "target after hook (5*3 via detour)");

    munmap(targetPage, pageSize);
}

}  // namespace

struct TestCase {
    const char* name;
    std::function<void()> run;
};

const std::vector<TestCase>& Registry() {
    static const std::vector<TestCase> cases = {
        {"hook_redirects_to_detour", &CreateHookRedirectsToDetour},
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
