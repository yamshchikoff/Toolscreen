# Смена разрешения по хоткеям — как в Windows

## Дефолтные хоткеи (`src/config/default.toml`)

| Клавиша | VK | mainMode | secondaryMode |
|---------|-----|----------|---------------|
| Z (90) | `VK_KEY_Z` | Fullscreen | Thin |
| J (74) | `VK_KEY_J` | Fullscreen | EyeZoom |
| LAlt (164) | `VK_LMENU` | Fullscreen | Wide |

Каждая клавиша — **toggle**: переключает между `mainMode` и `secondaryMode`.

Exclusion-клавиша для всех: RightAlt (114).

## Механизм (end-to-end)

```
WM_KEYDOWN (Z)
  → SubclassedWndProc (input_hook.cpp:5068)
    → HandleHotkeys (input_hook.cpp:2010)
      → CheckHotkeyMatch — сверка комбинации
      → SwitchToMode("Thin") (utils.cpp:1523)
        → StartModeTransition (render.cpp:9780)
          → RequestWindowClientResize(hwnd, W, H) (utils.cpp:3424)
            → PostMessage(WM_SIZE, SIZE_RESTORED, MAKELPARAM(W, H))
              → HandleWmSizeModeDimensions (input_hook.cpp:1958)
                → CallWindowProc(original WndProc)
                  → LWJGL получает WM_SIZE → пересоздаёт фреймбуфер
```

## Режимы (ModeConfig)

Каждый режим задаёт:
- `width`, `height` — целевое разрешение
- `stretch` — растяжение
- `gameTransition` — анимация (Cut / Bounce)
- `transitionDurationMs` — длительность

## Связанные файлы

| Файл | Что |
|------|-----|
| `src/config/default.toml` | Дефолтные хоткеи |
| `src/hooks/input_hook.cpp` | WndProc, HandleHotkeys |
| `src/common/utils.cpp` | SwitchToMode, RequestWindowClientResize |
| `src/render/render.cpp` | StartModeTransition |
| `src/gui/gui.h` | HotkeyConfig, ModeConfig |
