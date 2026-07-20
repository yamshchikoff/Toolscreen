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
PTRACE_OK=0
gdb --batch -ex "q" --pid "$PID" 2>/dev/null && PTRACE_OK=1

if [ "$PTRACE_OK" -eq 0 ]; then
    echo "ptrace ограничен. Включаю..."
    if sudo sysctl -w kernel.yama.ptrace_scope=0 2>/dev/null; then
        echo "ptrace разблокирован (до перезагрузки)"
    else
        echo "Запускаю инжектор с sudo..."
        sudo "$0" "$@"
        exit $?
    fi
fi

# Инжект через gdb: dlopen с флагом RTLD_NOW
RESULT=$(gdb --pid "$PID" \
    -batch \
    -ex "call (void*)dlopen(\"$SO\", 2)" \
    -ex "detach" \
    -ex "quit" 2>&1)

if echo "$RESULT" | grep -q "0x"; then
    echo "Внедрение успешно! Хэндл: $(echo "$RESULT" | grep '= (void')"
elif echo "$RESULT" | grep -q "No such file"; then
    echo "Ошибка: .so не найден процессом Minecraft. Проверь путь."
    exit 1
else
    echo "Результат gdb:"
    echo "$RESULT" | tail -3
fi

echo ""
echo "Лог: grep Toolscreen ~/toolscreen.log (пишет в stderr игры)"