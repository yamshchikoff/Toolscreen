#!/bin/bash
# Чинит обе Java-обёртки — системную и встроенную TLauncher.
# Запускать без sudo (для системной попросит пароль через sudo).
set -euo pipefail

SO_PATH="/home/user/Toolscreen/out/build/linux-test/bin/libtoolscreen.so"
LOG_PATH="/home/user/toolscreen.log"

if [ ! -f "$SO_PATH" ]; then
    echo "Ошибка: $SO_PATH не найден."
    echo "Собери проект: cmake --build /home/user/Toolscreen/out/build/linux-test"
    exit 1
fi

echo "=== $(date) ==="

# ---- Встроенная Java TLauncher (без sudo) ----
TLAUNCHER_JAVA="/home/user/.tlauncher/starter/jre_default/jre-21.0.11-linux-x64/bin/java"
TLAUNCHER_REAL="${TLAUNCHER_JAVA}.real"

if [ -f "$TLAUNCHER_JAVA" ]; then
    if [ ! -f "$TLAUNCHER_REAL" ]; then
        echo "[TLauncher] Сохраняю java → java.real..."
        mv "$TLAUNCHER_JAVA" "$TLAUNCHER_REAL"
    fi
    cat > "$TLAUNCHER_JAVA" << EOF
#!/bin/bash
export LD_PRELOAD="$SO_PATH"
exec "$TLAUNCHER_REAL" "\$@" 2>>"$LOG_PATH"
EOF
    chmod +x "$TLAUNCHER_JAVA"
    echo "[TLauncher] OK"
else
    echo "[TLauncher] Пропущено — нет $TLAUNCHER_JAVA"
fi

# ---- Системная Java (нужен sudo) ----
SYS_JAVA="/usr/lib/jvm/java-21-openjdk-amd64/bin/java"
SYS_REAL="${SYS_JAVA}.real"

if [ -f "$SYS_JAVA" ]; then
    echo "[System] Нужен пароль sudo..."
    if ! sudo true 2>/dev/null; then
        echo "[System] Пропущено — нет sudo"
    else
        if [ ! -f "$SYS_REAL" ]; then
            sudo mv "$SYS_JAVA" "$SYS_REAL"
        fi
        sudo tee "$SYS_JAVA" > /dev/null << EOF
#!/bin/bash
export LD_PRELOAD="$SO_PATH"
exec "$SYS_REAL" "\$@" 2>>"$LOG_PATH"
EOF
        sudo chmod +x "$SYS_JAVA"
        echo "[System] OK"
    fi
else
    echo "[System] Пропущено — нет $SYS_JAVA"
fi

echo "=== Готово ==="
echo "Лог: $LOG_PATH"