#pragma once

#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#endif

struct CursorTrailConfig;

void RenderCursorTrail(HWND hwnd, int windowWidth, int windowHeight, const CursorTrailConfig& cfg, uint64_t frameTag = 0);
