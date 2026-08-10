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
