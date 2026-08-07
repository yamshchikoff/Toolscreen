#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "platform/linux/resize_thread.h"

namespace {

int g_failures = 0;

void CheckBool(bool actual, bool expected, const std::string& label) {
    if (actual != expected) {
        std::cerr << "  FAIL: " << label << " expected " << (expected ? "true" : "false")
                  << " got " << (actual ? "true" : "false") << "\n";
        ++g_failures;
    }
}

void CheckGe(int actual, int expected, const std::string& label) {
    if (actual < expected) {
        std::cerr << "  FAIL: " << label << " expected >=" << expected
                  << " got " << actual << "\n";
        ++g_failures;
    }
}

void EnqueueIncrementsProcessedCount() {
    ResizeThread::Start();

    int before = ResizeThread::GetProcessedCount();
    ResizeThread::Enqueue(0x12345678, 800, 600);

    // Ждём пока поток обработает запрос (до 2 секунд)
    for (int i = 0; i < 200; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (ResizeThread::GetProcessedCount() > before) break;
    }

    int after = ResizeThread::GetProcessedCount();
    CheckGe(after, before + 1, "processed count should increase after enqueue");

    ResizeThread::Stop();
}

void MultipleEnqueuesProcessed() {
    ResizeThread::Start();

    int before = ResizeThread::GetProcessedCount();
    ResizeThread::Enqueue(1, 100, 200);
    ResizeThread::Enqueue(2, 300, 400);
    ResizeThread::Enqueue(3, 500, 600);

    for (int i = 0; i < 200; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (ResizeThread::GetProcessedCount() >= before + 3) break;
    }

    int after = ResizeThread::GetProcessedCount();
    CheckGe(after, before + 3, "processed count should be >=3 after 3 enqueues");

    ResizeThread::Stop();
}

void StopAndRestart() {
    ResizeThread::Start();
    ResizeThread::Stop();
    // После Stop повторный вызов Stop — безвреден
    ResizeThread::Stop();
    // Перезапуск
    ResizeThread::Start();
    ResizeThread::Enqueue(0x42, 640, 480);

    for (int i = 0; i < 200; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (ResizeThread::GetProcessedCount() > 0) break;
    }

    CheckGe(ResizeThread::GetProcessedCount(), 1,
            "should process after stop+restart");
    ResizeThread::Stop();
}

}  // namespace

struct TestCase {
    const char* name;
    std::function<void()> run;
};

const std::vector<TestCase>& Registry() {
    static const std::vector<TestCase> cases = {
        {"enqueue_increments_processed_count", &EnqueueIncrementsProcessedCount},
        {"multiple_enqueues_processed", &MultipleEnqueuesProcessed},
        {"stop_and_restart", &StopAndRestart},
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
