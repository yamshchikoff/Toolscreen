#!/bin/bash
# Инжектор libtoolscreen.so в запущенный Minecraft.
# Никаких подмен Java, никаких врапперов, никаких LD_PRELOAD.
# Просто: sudo ./scripts/inject.sh
set -e

SO="/home/user/Toolscreen/out/build/linux-test/bin/libtoolscreen.so"

if [ "$(id -u)" -ne 0 ]; then
    echo "Нужен root. Запусти: sudo $0"
    exit 1
fi

if [ ! -f "$SO" ]; then
    echo "Ошибка: $SO не собран."
    exit 1
fi

# Ищем процесс Minecraft — тот что в .minecraft/runtime
PID=$(pgrep -f "runtime/jre-legacy.*java" | head -1)

if [ -z "$PID" ]; then
    echo "Minecraft не найден. Запусти игру и зайди в мир."
    exit 1
fi

echo "PID: $PID"

# Включаем coredump для процесса
prlimit --pid "$PID" --core=unlimited

# Сохраняем и заменяем core_pattern (apport отбрасывает пользовательские корки)
OLD_PATTERN=$(cat /proc/sys/kernel/core_pattern)
echo "/tmp/core.%p.%t" > /proc/sys/kernel/core_pattern
echo "Core dumps enabled → /tmp/core.*"

# Восстанавливаем core_pattern при выходе
trap "echo '$OLD_PATTERN' > /proc/sys/kernel/core_pattern 2>/dev/null" EXIT

echo "Инжект..."

# Разрешаем ptrace
sysctl -w kernel.yama.ptrace_scope=0 > /dev/null 2>&1

# Инжектим через gdb: dlopen с флагом RTLD_NOW=2
gdb --pid "$PID" \
    -batch \
    -ex "call (void*)dlopen(\"libXtst.so.6\", 2)" \
    -ex "call (void*)dlopen(\"$SO\", 2)" \
    -ex "call (char*)dlerror()" \
    -ex "detach" \
    -ex "quit" 2>&1 | grep -E "0x|Error|error|symbol|dlopen"

echo ""
echo "Готово. Вернись в игру — должно появиться окно ImGui."