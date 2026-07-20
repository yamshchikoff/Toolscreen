#pragma once

#include "platform/platform_types.h"

// ---- Shared initialization logic ----
// Code that runs on both Windows and Linux during module startup.
// Extracted from dllmain.cpp to avoid duplication.

#include <string>
#include <atomic>

// Forward declarations
struct Config;

namespace SharedInit {

// Initialize config (load from file or embedded defaults)
bool InitConfig(Config& config, const std::string& toolscreenPath);

// Initialize translations/locale
bool InitTranslations(const Config& config);

// Initialize logging subsystem
bool InitLogging(const std::string& toolscreenPath);

// Start background threads (file monitor, image monitor, etc.)
void StartBackgroundThreads();

// Stop all background threads
void StopBackgroundThreads();

// Install global exception handlers (SEH on Windows, signal handlers on Linux)
void InstallExceptionHandlers();

// Load bundled fonts from the module resources
bool LoadBundledFonts(const std::string& toolscreenPath);

// Get the path to the toolscreen config directory
std::string GetConfigDirectory();

// Get the path to the toolscreen data directory
std::string GetDataDirectory();

} // namespace SharedInit
