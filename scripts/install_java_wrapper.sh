#!/bin/bash
# Устанавливает обёртку для системной Java, чтобы LD_PRELOAD
# автоматически подгружался при любом запуске Java (Minecraft).
# Требует sudo. Запускать из корня проекта: sudo ./scripts/install_java_wrapper.sh

set -e

SO_PATH="/home/user/Toolscreen/out/build/linux-test/bin/libtoolscreen.so"
JAVA_PATH="/usr/lib/jvm/java-21-openjdk-amd64/bin/java"
LOG_PATH="/home/user/toolscreen.log"

if [ ! -f "$SO_PATH" ]; then
    echo "Ошибка: $SO_PATH не найден. Сначала собери проект: cmake --build out/build/linux-test"
    exit 1
fi

if [ -f "$JAVA_PATH.real" ]; then
    echo "Обёртка уже установлена. Обновляю скрипт..."
else
    echo "Устанавливаю обёртку..."
    mv "$JAVA_PATH" "$JAVA_PATH.real"
fi

cat > "$JAVA_PATH" << EOF
#!/bin/bash
export LD_PRELOAD=$SO_PATH
exec "\$(dirname "\$0")/java.real" "\$@" 2>>$LOG_PATH
EOF

chmod +x "$JAVA_PATH"
echo "Готово. Лог: $LOG_PATH"
echo "Проверка: java -version (должен показать [Toolscreen])"