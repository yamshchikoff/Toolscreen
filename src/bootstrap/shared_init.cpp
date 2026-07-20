#include "shared_init.h"
#include "gui/gui.h"
#include "config/config_toml.h"
#include "common/profiler.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef PLATFORM_LINUX
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include <cxxabi.h>
#endif

namespace SharedInit {

namespace {

std::atomic<bool> g_configLoaded{false};
std::atomic<bool> g_translationsLoaded{false};

// ---- Linux signal handler (replaces Windows SEH) ----
#ifdef PLATFORM_LINUX
void LinuxSignalHandler(int sig, siginfo_t* info, void* /*ctx*/) {
    // Async-signal-safe only: write(), backtrace_symbols_fd(), signal(), raise()
    // NO fprintf, malloc, free, backtrace_symbols — would deadlock if signal
    // arrived while another thread held the malloc lock.

    const char* sigName = "UNKNOWN";
    switch (sig) {
        case SIGSEGV: sigName = "SIGSEGV"; break;
        case SIGABRT: sigName = "SIGABRT"; break;
        case SIGFPE:  sigName = "SIGFPE"; break;
        case SIGILL:  sigName = "SIGILL"; break;
        case SIGBUS:  sigName = "SIGBUS"; break;
    }

    // Build message with write() — no malloc, no fprintf
    char buf[256];
    int len = snprintf(buf, sizeof(buf),
                       "[Toolscreen] FATAL: signal %d (%s) addr=%p\n",
                       sig, sigName, info ? info->si_addr : nullptr);
    if (len > 0) write(STDERR_FILENO, buf, static_cast<size_t>(len));

    // Print stack trace using backtrace_symbols_fd — single syscall, no malloc
    void* trace[32];
    int traceSize = backtrace(trace, 32);
    backtrace_symbols_fd(trace, traceSize, STDERR_FILENO);

    // Re-raise with default handler
    signal(sig, SIG_DFL);
    raise(sig);
}

void InstallLinuxSignalHandlers() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = LinuxSignalHandler;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);

    // Block these signals in other threads
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGSEGV);
    sigaddset(&mask, SIGABRT);
    sigaddset(&mask, SIGFPE);
    pthread_sigmask(SIG_UNBLOCK, &mask, nullptr);
}
#endif

} // namespace

bool InitConfig(Config& config, const std::string& toolscreenPath) {
    if (g_configLoaded.load()) return true;

    std::string configPath = toolscreenPath + "/toolscreen.toml";

    // Try to load existing config
    if (std::filesystem::exists(configPath)) {
        // Load from file using our TOML parser
        // For Linux, convert path to UTF-8 (no wstring needed)
        std::ifstream file(configPath);
        if (file.good()) {
            std::string tomlContent((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
            try {
                toml::table tbl = toml::parse(tomlContent);
                ConfigFromToml(tbl, config);
                g_configLoaded.store(true);
                fprintf(stderr, "[Toolscreen] Config loaded from %s\n", configPath.c_str());
                return true;
            } catch (const std::exception& e) {
                fprintf(stderr, "[Toolscreen] Config parse error: %s\n", e.what());
            }
        }
    }

    // Fall back to embedded defaults
    fprintf(stderr, "[Toolscreen] Using embedded default config\n");
    LoadEmbeddedDefaultConfig(config);
    g_configLoaded.store(true);
    return true;
}

bool InitTranslations(const Config& config) {
    if (g_translationsLoaded.load()) return true;

    // TODO: Load translation from bundled resource or lang/ directory
    g_translationsLoaded.store(true);
    return true;
}

bool InitLogging(const std::string& toolscreenPath) {
    // Set up log file
    std::string logPath = toolscreenPath + "/toolscreen.log";
    fprintf(stderr, "[Toolscreen] Log file: %s\n", logPath.c_str());

    // Log file will be opened by the background log thread
    return true;
}

void StartBackgroundThreads() {
    // TODO: Start file monitor thread, image monitor thread
}

void StopBackgroundThreads() {
    // TODO: Signal all threads to stop, join them
}

void InstallExceptionHandlers() {
#ifdef PLATFORM_LINUX
    InstallLinuxSignalHandlers();
    fprintf(stderr, "[Toolscreen] Signal handlers installed\n");
#endif
}

bool LoadBundledFonts(const std::string& toolscreenPath) {
    // Check for fonts in the data directory
    std::string fontsDir = toolscreenPath + "/fonts";
    if (std::filesystem::exists(fontsDir)) {
        fprintf(stderr, "[Toolscreen] Found fonts directory: %s\n", fontsDir.c_str());
    }
    return true;
}

std::string GetConfigDirectory() {
#ifdef PLATFORM_LINUX
    const char* xdgConfig = getenv("XDG_CONFIG_HOME");
    if (xdgConfig && xdgConfig[0]) {
        return std::string(xdgConfig) + "/toolscreen";
    }
    const char* home = getenv("HOME");
    if (home && home[0]) {
        return std::string(home) + "/.config/toolscreen";
    }
#endif
    return ".toolscreen";
}

std::string GetDataDirectory() {
#ifdef PLATFORM_LINUX
    const char* xdgData = getenv("XDG_DATA_HOME");
    if (xdgData && xdgData[0]) {
        return std::string(xdgData) + "/toolscreen";
    }
    const char* home = getenv("HOME");
    if (home && home[0]) {
        return std::string(home) + "/.local/share/toolscreen";
    }
#endif
    return ".toolscreen";
}

} // namespace SharedInit
