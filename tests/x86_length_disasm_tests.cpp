#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "platform/linux/x86_length_disasm.h"

namespace {

int g_failures = 0;

void CheckEq(size_t actual, size_t expected, const std::string& label) {
    if (actual != expected) {
        std::cerr << "  FAIL: " << label << " expected " << expected
                  << " got " << actual << "\n";
        ++g_failures;
    }
}

// endbr64 (F3 0F 1E FA) = 4 байта → покрывает minBytes=4
void Endbr64ExactFour() {
    uint8_t code[] = { 0xF3, 0x0F, 0x1E, 0xFA, 0x55 }; // endbr64 + push rbp
    CheckEq(X86InsnMinCover(code, 4, 16), 4, "endbr64 covers 4");
}

// endbr64(4) + push rbp(1) = 5 → покрывает minBytes=5
void Endbr64PlusPushCovers5() {
    uint8_t code[] = { 0xF3, 0x0F, 0x1E, 0xFA, 0x55, 0x48, 0x89, 0xE5 };
    CheckEq(X86InsnMinCover(code, 5, 16), 5, "endbr64+push covers 5");
}

// push rbp(1) + mov rsp,rbp(3) = 4 → покрывает minBytes=3
void PushPlusMovCovers3() {
    uint8_t code[] = { 0x55, 0x48, 0x89, 0xE5, 0x41, 0x54 };
    CheckEq(X86InsnMinCover(code, 3, 16), 4, "push+mov rsp,rbp covers 3");
}

// mov 0x968(%rdi),%rax = 48 8B 87 68 09 00 00 (7 байт)
void MovWithDisp32Is7Bytes() {
    uint8_t code[] = { 0x48, 0x8B, 0x87, 0x68, 0x09, 0x00, 0x00, 0x48 };
    CheckEq(X86InsnMinCover(code, 1, 16), 7, "mov [rdi+0x968],rax is 7 bytes");
}

// test rax,rax(3) + je rel8(2) = 5 → покрывает minBytes=5
void TestPlusJeCovers5() {
    uint8_t code[] = { 0x48, 0x85, 0xC0, 0x74, 0x02, 0xFF, 0x10 };
    CheckEq(X86InsnMinCover(code, 5, 16), 5, "test+je covers 5");
}

// Полный пролог XNextEvent: endbr64(4)+push(1) = 5 → покрывает minBytes=5
void XNextEventPrologueCovers5() {
    uint8_t code[] = {
        0xF3, 0x0F, 0x1E, 0xFA, // endbr64
        0x55,                     // push rbp
        0x48, 0x89, 0xE5,         // mov rsp,rbp
        0x41, 0x54,               // push r12
        0x49, 0x89, 0xFC,         // mov rdi,r12
        0x53,                     // push rbx
        0x48, 0x8B, 0x87, 0x68, 0x09, 0x00, 0x00 // mov 0x968(%rdi),rax
    };
    CheckEq(X86InsnMinCover(code, 5, 32), 5, "XNextEvent prologue covers 5");
}

}  // namespace

struct TestCase {
    const char* name;
    std::function<void()> run;
};

const std::vector<TestCase>& Registry() {
    static const std::vector<TestCase> cases = {
        {"endbr64_exact_four",   &Endbr64ExactFour},
        {"endbr64_plus_push_5",  &Endbr64PlusPushCovers5},
        {"push_plus_mov_3",      &PushPlusMovCovers3},
        {"mov_disp32_7bytes",    &MovWithDisp32Is7Bytes},
        {"test_plus_je_5",       &TestPlusJeCovers5},
        {"xnext_prologue_5",     &XNextEventPrologueCovers5},
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
