# Сборка Toolscreen на Ubuntu

## Зависимости

```bash
sudo apt install -y build-essential cmake g++ \
    libx11-dev libxext-dev libxi-dev libxfixes-dev \
    libxcursor-dev libxrandr-dev libxinerama-dev libxtst-dev \
    libgl1-mesa-dev libglew-dev
```

## Сборка

```bash
git clone git@github.com:yamshchikoff/Toolscreen.git
cd Toolscreen
mkdir -p out/build/linux && cd out/build/linux
cmake ../.. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(nproc)
```

Результат: `bin/libtoolscreen.so`

## Включение ImGui

По умолчанию оверлей выключен. Для включения добавить флаг при конфигурации:

```bash
cmake ../.. -DCMAKE_BUILD_TYPE=Release -DTOOLSCREEN_ENABLE_IMGUI=ON
```

## Запуск

Скопировать `scripts/inject.sh` и `scripts/save_core.sh` рядом с `libtoolscreen.so`, запустить:

```bash
sudo ./inject.sh
```

## Конфигурация биндов ресайза

Клавиши ресайза читаются из `~/.toolscreen/resize_bindings.json`.

Скопировать эталон из репозитория:

```bash
mkdir -p ~/.toolscreen
cp src/platform/linux/resize_bindings.json ~/.toolscreen/resize_bindings.json
```

Формат записи:

```json
{
    "hotkeys": [
        { "key": "Z",    "keycode": 52, "mode": "Thin",    "width": "...", "height": "..." },
        { "key": "J",    "keycode": 44, "mode": "EyeZoom", "width": 384,  "height": 16384 },
        { "key": "LAlt", "keycode": 64, "mode": "Wide",    "width": "...", "height": 0.25 }
    ]
}
```

- `key` — имя кнопки (`"Z"`, `"J"`, `"LAlt"`, `"F11"`, `"/"`). Если задано — keycode резолвится по имени, `keycode` игнорируется.
- `keycode` — числовой X11 keycode, используется только если `key` отсутствует.
- `width` / `height` — число или формула (`screenWidth`, `screenHeight`, `max`, `roundEven`).
- Каждая клавиша — toggle: первое нажатие переводит в режим, повторное — возвращает fullscreen.

