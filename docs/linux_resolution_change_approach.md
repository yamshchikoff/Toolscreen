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

**Вывод**: X11-функции ресайза окна (XResizeWindow, XSendEvent, XMoveResizeWindow, XPutBackEvent, XConfigureWindow, EWMH) не работают на окне Minecraft/LWJGL.

### Ресайз мышкой — работает

Пользователь может изменить размер окна Minecraft мышкой (drag за край/угол окна). WM ресайзит окно, LWJGL/GLFW получает событие и пересоздаёт фреймбуфер. **Это работает штатно.**

**Нужно понять, что именно происходит при ресайзе мышкой**, и воспроизвести ту же цепочку событий из инжектора:

1. Какие X11-события получает окно при ресайзе мышкой? (`ConfigureNotify`? Размер в PropertyNotify/`_NET_FRAME_EXTENTS`?)
2. Как LWJGL/GLFW узнаёт о новом размере? (Обработчик `ConfigureNotify` в GLFW?)
3. Можно ли воспроизвести ту же цепочку событий через XSendEvent/XPutBackEvent с правильными параметрами?

**Подход**: перехватить X11-события окна в момент ресайза мышкой, зафиксировать их последовательность через `dprintf` в GDB, и воспроизвести.

**Исправлено**: `[[gnu::noinline]] static void DoTestResize()` — компилятор больше не выкидывает. Подтверждено через `objdump` (call на offset 0x188).

**GDB dprintf**: `DoTestResize` вызывается (3 срабатывания). Z ловится, код выполняется. Но `XMoveResizeWindow` не ресайзит окно Minecraft.

### Ресайз из внешнего процесса — работает ✅

**2026-08-07**: Написана программа `/tmp/resize_mc.c`, которая вызывает `XResizeWindow` и `XConfigureWindow` на окне Minecraft **из другого процесса**. Оба метода **сработали** — размер окна изменился (925×530 → 800×500 → 700×400), `XGetWindowAttributes` подтверждает новый размер.

**Ключевой вывод**: проблема не в X11 и не в свойствах окна Minecraft. Проблема в том, что мы вызываем ресайз **на том же Display-соединении**, которым владеет GLFW. GLFW видит вызов в своей очереди и игнорирует/перезаписывает. Внешняя программа `/tmp/resize_mc` работает, потому что открывает **новое** Display-соединение — для GLFW это внешнее событие, неотличимое от WM.

**Гипотеза**: если из инжектора открыть **второе** Display-соединение (`XOpenDisplay`) и вызвать `XResizeWindow` через него, ресайз сработает.

### План теста: ресайз через второе Display-соединение

**Почему должно сработать**:
- `/tmp/resize_mc` доказал: X11-функции ресайза работают на окне Minecraft из другого соединения
- Второе соединение для процесса — легальная операция в X11
- GLFW не контролирует чужие Display-соединения и не может отличить их от WM
- X-сервер сам разрулит синхронизацию между соединениями

**Что сделать**:
1. В `x11_window.cpp`: добавить `Display* g_ownDpy = XOpenDisplay(...)` — второе соединение
2. В `RequestWindowResize`: вызывать `XResizeWindow(g_ownDpy, win, w, h)` вместо `dpy` из GLFW
3. Вызвать из `DetourXNextEvent` (под локом или нет — не важно, соединение другое)
4. Заинжектить, нажать LAlt, проверить: окно ресайзнулось?

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

### Ресайз через FindTopLevelWindow + _NET_CLIENT_LIST — работает ✅

**2026-08-07**: До этого ресайз вызывался на окне, полученном через `glXGetCurrentDrawable()` — но это GLX-дочернее окно (`0x2e0000b`), а не топлевел (`0x2e00009`). `XConfigureWindow` на GLX child падает с BadWindow.

**Попытка 1**: `FindTopLevelWindow` через `XQueryTree` (подъём по дереву до `WM_STATE`). **НЕ СРАБОТАЛО**: `XQueryTree` падает на GLX-окнах — они не в обычном X11-дереве.

**Попытка 2**: `FindTopLevelWindow` через `_NET_CLIENT_LIST` (как внешняя программа `resize_gui.c`). Запрашиваем список топлевел-окон у WM, фильтруем по `WM_CLASS` содержащему "Minecraft". **СРАБОТАЛО**: ресайз через `XConfigureWindow` на втором Display-соединении (`g_ownDpy`) успешно меняет размер окна Minecraft из инжектора.

**Ключевой вывод**: для ресайза нужно использовать топлевел-окно из `_NET_CLIENT_LIST`, а не GLX-дочернее из `glXGetCurrentDrawable()`.

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
