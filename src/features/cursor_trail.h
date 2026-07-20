#pragma once

#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#elif defined(PLATFORM_LINUX)
#include "platform/platform_types.h"
#endif

struct CursorTrailConfig;

void RenderCursorTrail(HWND hwnd, int windowWidth, int windowHeight, const CursorTrailConfig& cfg, uint64_t frameTag = 0);
