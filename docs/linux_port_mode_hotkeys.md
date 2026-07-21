# Состояние порта «Mode Hotkeys» на Linux

> Оценка готовности: **~10-15%**

Весь код **компилируется** на Linux (благодаря заглушкам Windows API в `platform_types.h`), но **не работает** — критическая цепочка вызовов разорвана в четырёх местах.

---

## 1. `IsResolutionChangeSupported()` — ✅ Работает

**Файл:** `src/common/version.cpp:149`

Чистая проверка версии Minecraft (≥ 1.13). Никаких платформенных зависимостей.

---

## 2. `SwitchToMode()` — 💀 Мёртвый код

**Файл:** `src/common/utils.cpp:1523`

Скомпилирован, платформенно-независимый код, но **никогда не вызывается** на Linux. Единственные места вызова:
- `HandleHotkeys()` — хоткеи не работают (см. п. 4)
- `logic_thread.cpp` — logic thread не запущен (см. п. 3)
- `input_hook.cpp` — hold-release хоткеи не работают

---

## 3. `RequestWindowClientResize()` — 🪫 Заглушка

**Файл:** `src/common/utils.cpp:3424`

На Windows отправляет `PostMessage(WM_SIZE, ...)`. На Linux `PostMessage` — заглушка:
```cpp
// platform_types.h:727
inline int PostMessage(void*, unsigned int, WPARAM, LPARAM) { return 1; }
```

**Что already есть:** `X11Window::RequestWindowResize()` в `src/platform/linux/x11_window.cpp:146`, который вызывает `XResizeWindow()`. Он готов, но не подключён к `RequestWindowClientResize()`.

**Нужно:** `#ifdef PLATFORM_LINUX` в `utils.cpp:3441` — вместо `PostMessage` дёргать `X11Window::RequestWindowResize()`.

---

## 4. `HandleHotkeys()` — 💀 Мёртвый код

**Файл:** `src/hooks/input_hook.cpp:2010`

Скомпилирован (весь `src/hooks/` собирается glob'ом), но **никогда не вызывается**:
- `SubclassedWndProc` не установлен на Linux (`dllmain.cpp` исключён из сборки)
- X11-события клавиатуры идут только в ImGui через `RouteX11EventToImGui()` в `glx_hook.cpp:244`
- Нет маршрута из X11-событий в `HandleHotkeys()`

**Нужно:** добавить вызов `SubclassedWndProc` (или Linux-эквивалента) в цепочку обработки X11-событий.

---

## 5. `StartModeTransition()` — 🟡 Частично

**Файл:** `src/render/render.cpp:9688`

Машина состояний анимации (мьютексы, прогресс перехода, viewport snapshot) работает. Но финальный шаг — `RequestWindowClientResize()` — заглушка, поэтому реального ресайза окна не происходит.

---

## 6. `HandleWmSizeModeDimensions()` — 💀 Мёртвый код

**Файл:** `src/hooks/input_hook.cpp:1958`

Вызывается только из `SubclassedWndProc()`, который не установлен на Linux.

---

## 7. `hk_glViewport` — 🪫 Заглушка (pass-through)

**Файл:** `src/platform/linux/glx_hook.cpp:642`

Хук `glViewport` работает (через `dlsym(RTLD_NEXT)`), но содержит `// TODO: Mode viewport override logic from dllmain.cpp` и просто пробрасывает оригинальные параметры без изменений.

**Нужно:** реализовать viewport clamping — зажимать `glViewport` до размеров текущего режима из `CachedModeViewport`, включая stretch.

---

## 8. `RecalculateModeDimensions()` — 💀 Скомпилирован, не вызван

**Файл:** `src/common/mode_dimensions.cpp:98`

Полностью кроссплатформенный код. Вызывается из `UpdateCachedScreenMetrics()` в logic thread, но logic thread не запущен.

---

## 9. `UpdateCachedViewportMode()` — 💀 Скомпилирован, не вызван

**Файл:** `src/runtime/logic_thread.cpp:439`

Кроссплатформенный код. Вызывается из `LogicThreadFunc()`, но logic thread не запущен.

`StartThreads()` в `linux_main.cpp:344` содержит:
```cpp
// TODO: Start logic thread, file monitor, image monitor
// These will be connected in later phases
```
и **не вызывает** `StartLogicThread()`.

---

## 10. X11 input handling — 🟡 Только ImGui

**Файлы:** `src/platform/linux/x11_input.cpp`, `src/gui/imgui_impl_x11.cpp`

Полноценный X11-бэкенд: клавиши, мышь, колёсико, фокус, ресайз окна — всё ловится и обрабатывается. Но события идут **только в ImGui** (`RouteX11EventToImGui` → `ImGui_ImplX11_HandleKeyEvent/HandleMouseButtonEvent/...`). Для хоткеев нужна вторая ветка — в `HandleHotkeys()`.

---

## Что нужно для работающей функциональности

### Шаг 1. Запустить logic thread
`linux_main.cpp:344` — вызвать `StartLogicThread()` в `StartThreads()`.

### Шаг 2. Реализовать `RequestWindowClientResize()` на Linux
`utils.cpp:3441` — добавить `#ifdef PLATFORM_LINUX`:
```cpp
#ifdef PLATFORM_LINUX
X11Window::RequestWindowResize(width, height);
#else
PostMessage(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(width, height));
#endif
```

### Шаг 3. Подключить X11-события к хоткеям
В `glx_hook.cpp` (или новом файле) — добавить вызов `HandleHotkeys()` (или Linux-эквивалента `SubclassedWndProc`) при получении клавиатурных событий из X11.

### Шаг 4. Реализовать viewport clamping в `hk_glViewport`
`glx_hook.cpp:642` — заменить TODO на логику из Windows-версии:
- Читать `CachedModeViewport` из lock-free буфера
- Если active mode задаёт размеры — зажимать `glViewport(x, y, width, height)` до mode dimensions + stretch

### Полная цепочка после исправлений

```
X11 KeyPress
  → X11Input::PollEvents()
    → RouteX11EventToImGui()        // ImGui (уже работает)
    → HandleHotkeys()               // хоткеи (добавить)
      → SwitchToMode("Thin")
        → StartModeTransition()
          → RequestWindowClientResize(W, H)
            → XResizeWindow(W, H)   // вместо PostMessage
              → LWJGL пересоздаёт фреймбуфер
                → hk_glViewport clamp (новый код)
```

---

## Сводная таблица

| # | Функция | Файл | Состояние |
|---|---------|------|-----------|
| 1 | `SwitchToMode()` | `utils.cpp:1523` | 💀 Мёртвый код (не вызывается) |
| 2 | `RequestWindowClientResize()` | `utils.cpp:3424` | 🪫 Заглушка (PostMessage no-op) |
| 3 | `HandleHotkeys()` | `input_hook.cpp:2010` | 💀 Мёртвый код (WndProc не установлен) |
| 4 | `StartModeTransition()` | `render.cpp:9688` | 🟡 Частично (анимация без ресайза) |
| 5 | `HandleWmSizeModeDimensions()` | `input_hook.cpp:1958` | 💀 Мёртвый код |
| 6 | `hk_glViewport` | `glx_hook.cpp:642` | 🪫 Заглушка (TODO, pass-through) |
| 7 | `RecalculateModeDimensions()` | `mode_dimensions.cpp:98` | 💀 Скомпилирован, не вызван |
| 8 | `IsResolutionChangeSupported()` | `version.cpp:149` | ✅ Работает |
| 9 | `UpdateCachedViewportMode()` | `logic_thread.cpp:439` | 💀 Скомпилирован, не вызван |
| 10 | X11 input handling | `x11_input.cpp` | 🟡 ImGui only, без хоткеев |
