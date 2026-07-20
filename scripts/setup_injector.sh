#!/bin/bash
# Оборачивает Java рантайм Minecraft для LD_PRELOAD инжекта.
# Без подмены системных файлов, без изменений TLauncher.
# Запуск: sudo ./scripts/setup_injector.sh
set -e

JAVA_RT="/home/user/.minecraft/runtime/jre-legacy/linux/jre-legacy/bin/java"
SO="/home/user/Toolscreen/out/build/linux-test/bin/libtoolscreen.so"

if [ ! -f "$SO" ]; then
    echo "Ошибка: $SO не собран. cmake --build out/build/linux-test"
    exit 1
fi

if [ ! -f "$JAVA_RT" ]; then
    echo "Ошибка: $JAVA_RT не найден. Запусти Minecraft хотя бы раз через TLauncher."
    exit 1
fi

# Сохраняем оригинал
if [ ! -f "${JAVA_RT}.real" ]; then
    cp "$JAVA_RT" "${JAVA_RT}.real"
    echo "Сохранён оригинал: ${JAVA_RT}.real"
fi

# Ставим обёртку
cat > "$JAVA_RT" << EOF
#!/bin/bash
export LD_PRELOAD="$SO"
exec "${JAVA_RT}.real" "\$@"
EOF
chmod +x "$JAVA_RT"

echo "Готово. Запускай Minecraft как обычно."
echo "Отмена: sudo mv ${JAVA_RT}.real ${JAVA_RT}"