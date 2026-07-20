#pragma once

// Minimal X11 backend for ImGui on Linux
// Replaces imgui_impl_glfw.cpp for LD_PRELOAD-injected scenarios
// where we don't control the window/GL context creation.

#ifdef PLATFORM_LINUX

#include <X11/Xlib.h>

// Initialize the X11 backend.
// display: the shared X11 Display
// window:   the game window to listen for input on
// Returns true on success.
bool ImGui_ImplX11_Init(Display* display, Window window);

// Shutdown the backend
void ImGui_ImplX11_Shutdown();

// Process pending X11 events and feed them to ImGui.
// Call once per frame before ImGui::NewFrame().
void ImGui_ImplX11_NewFrame();

// ---- Input hooks for ImGui ----
// These are called by our X11Input system when events occur on the game window.
// They convert platform events to ImGui's internal format.

bool ImGui_ImplX11_HandleKeyEvent(unsigned int keycode, bool isDown, unsigned int state);
bool ImGui_ImplX11_HandleCharEvent(unsigned int charCode);
bool ImGui_ImplX11_HandleMouseButtonEvent(int button, bool isDown);
bool ImGui_ImplX11_HandleMouseMotionEvent(int x, int y);
bool ImGui_ImplX11_HandleMouseWheelEvent(int delta);

#endif // PLATFORM_LINUX
