# Подход: Wide по LAlt на Linux

> **Scope**: только Wide (LAlt, keycode=64). Thin (Z) и EyeZoom (J) — потом.

## Что уже работает

- ✅ **Перехват клавиш**: inline-хук `XNextEvent` → `DetourXNextEvent` логирует KeyPress/KeyRelease
- ✅ **Хоткеи**: Z (keycode=52), J (keycode=44), LAlt (keycode=64)
- ✅ **Инжект**: `inject.sh` и gdbwrap
- ✅ **Игра стабильна**

## Что нужно сделать

### Шаг 1: минимальный тест ресайза

~~В `DetourXNextEvent`, при нажатии LAlt, дёрнуть `X11Window::RequestWindowResize(win, 800, 600)` напрямую.~~ **НЕ СРАБОТАЛО**: Z ловится, `RequestWindowResize` вызывается, но `XResizeWindow` игнорируется WM — окно не меняет размер.

**Следующая попытка**: `XSendEvent(ConfigureRequest)` — **НЕ СРАБОТАЛО**.
**Затем**: `XMoveResizeWindow` + синтетический `ConfigureNotify` — **НЕ СРАБОТАЛО**. GDB показал: код ресайза выкинут компилятором из бинарника (только fopen/vfprintf/fclose от HOOK_LOG, затем эпилог). Call к `RequestWindowResize` отсутствует. **Исправлено** `noinline DoTestResize`.

**Попытка**: `XPutBackEvent(ConfigureNotify)` — **НЕ СРАБОТАЛО**. GDB dprintf подтвердил: `RequestWindowResize` вызывается (hit count 2), XPutBackEvent исполняется. Но GLFW/LWJGL не реагирует на синтетический ConfigureNotify — фреймбуфер не пересоздаётся.

**Вывод**: X11-функции ресайза окна (XResizeWindow, XSendEvent, XMoveResizeWindow, XPutBackEvent) не работают на окне Minecraft/LWJGL. Следующий подход: viewport clamping через `hk_glViewport`.

**Исправлено**: `[[gnu::noinline]] static void DoTestResize()` — компилятор больше не выкидывает. Подтверждено через `objdump` (call на offset 0x188).

**GDB dprintf**: `DoTestResize` вызывается (3 срабатывания). Z ловится, код выполняется. Но `XMoveResizeWindow` не ресайзит окно Minecraft. Вывод: X11-функции ресайза не работают на этом окне.

Следующий шаг: `XConfigureWindow`?

**Файл**: `glx_hook.cpp` — DetourXNextEvent

**Риск**: `XResizeWindow` изнутри `XLockDisplay` может вызвать deadlock. **Проверено**: deadlock'а нет, игра не виснет.

### Шаг 2: починить RequestWindowClientResize на Linux

Добавить `#ifdef PLATFORM_LINUX`:
```cpp
bool RequestWindowClientResize(HWND hwnd, int width, int height, const char* source) {
#ifdef PLATFORM_LINUX
    Window win = X11Display::GetGameWindow();
    if (!win) return false;
    return X11Window::RequestWindowResize(win, width, height);
#else
    // существующий Windows-код с PostMessage
#endif
}
```

**Файл**: `src/common/utils.cpp:3424`

После этого вся цепочка `SwitchToMode → StartModeTransition → RequestWindowClientResize → XResizeWindow` заработает.

### Шаг 3: LAlt → SwitchToMode("Wide")

В `DetourXNextEvent`: если keycode == 64 и KeyPress → `SwitchToMode("Wide")`.

**Файл**: `glx_hook.cpp`

**Важно**: `SwitchToMode` вызывает `HOOK_LOG`, `LogCategory`, мьютексы — всё это функции. Внутри `DetourXNextEvent` мы под `XLockDisplay` (XNextEvent всегда под локом). Нужно проверить что вызовы из-под лока не вызывают deadlock.

Если deadlock — вынести вызов `SwitchToMode` из `DetourXNextEvent` в `hk_glXSwapBuffers` (рендер-поток), через очередь событий.

### Шаг 4: Z→Thin, J→EyeZoom — отдельными документами

Реализуются только после полного принятия Wide. Каждый — в отдельном документе и отдельным планом. В скоуп данного документа не входят.

### Шаг 5: viewport clamping (hk_glViewport)

Чтобы режимы EyeZoom и Thin работали корректно, `hk_glViewport` должен зажимать viewport до размеров целевого режима. Сейчас это TODO (`glx_hook.cpp:650`). Нужно перенести логику из Windows `hkglViewport` (`render.cpp`).

## Альтернативный путь (если deadlock)

Если `SwitchToMode` из `DetourXNextEvent` вызывает deadlock:

1. В `DetourXNextEvent`: сохранять keycode в thread-local/atomic переменную (без вызовов функций)
2. В `hk_glXSwapBuffers`: проверять переменную, вызывать `SwitchToMode` на рендер-потоке

Это безопаснее, но задерживает реакцию на 1 кадр.

## Файлы

| Файл | Что меняется |
|------|-------------|
| `src/platform/linux/glx_hook.cpp` | DetourXNextEvent — вызов ресайза/switchmode |
| `src/common/utils.cpp` | RequestWindowClientResize — Linux-реализация |
| `src/platform/linux/x11_window.cpp` | Уже готов (RequestWindowResize) |
| `src/platform/linux/x11_display.cpp` | Уже готов (GetGameWindow) |
