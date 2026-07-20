#!/bin/bash
# Внедряет libtoolscreen.so в работающий процесс Minecraft
# Использование: ./scripts/inject.sh

set -e

SO="/home/user/Toolscreen/out/build/linux-test/bin/libtoolscreen.so"

if [ ! -f "$SO" ]; then
    echo "Ошибка: $SO не найден. Собери проект: cmake --build out/build/linux-test"
    exit 1
fi

# Ищем процесс Minecraft
PID=$(pgrep -f "minecraft\|net.minecraft\|java.*minecraft\|org.tlauncher" 2>/dev/null | head -1)

if [ -z "$PID" ]; then
    echo "Ошибка: процесс Minecraft не найден."
    echo "Запусти Minecraft и повтори."
    exit 1
fi

echo "Найден PID: $PID"
echo "Внедряю $SO..."

# Проверяем доступность ptrace
if ! gdb --batch -ex "q" --pid "$PID" 2>/dev/null; then
    echo "Ошибка: нет доступа к процессу (ptrace)."
    echo "Отключи ограничения:"
    echo "  sudo sysctl -w kernel.yama.ptrace_scope=0"
    echo "Или запусти через sudo:"
    echo "  sudo $0"
    exit 1
fi

# Инжект через gdb: dlopen с флагом RTLD_NOW
gdb --pid "$PID" \
    -batch \
    -ex "call (void*)dlopen(\"$SO\", 2)" \
    -ex "detach" \
    -ex "quit" 2>&1 | grep -v "^$\|Reading symbols\|debuginfo\|(gdb)\|Detaching"

echo ""
echo "Готово. Проверь лог: grep Toolscreen ~/toolscreen.log"
echo ""
echo "Если GLEW не инициализировался — открой мир в Minecraft,"
echo ".so активируется при создании GL-контекста."