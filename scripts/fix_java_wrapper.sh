#!/bin/bash
# Чинит обе Java-обёртки — системную и встроенную TLauncher.
# Запускать без sudo (для системной попросит пароль через sudo).

SO_PATH="/home/user/Toolscreen/out/build/linux-test/bin/libtoolscreen.so"
LOG_PATH="/home/user/toolscreen.log"
SETUP_LOG="/home/user/toolscreen_setup.log"

exec > >(tee -a "$SETUP_LOG") 2>&1
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
export LD_PRELOAD=$SO_PATH
exec $TLAUNCHER_REAL "\$@" 2>>$LOG_PATH
EOF
    chmod +x "$TLAUNCHER_JAVA"
    echo "[TLauncher] Обёртка: $TLAUNCHER_JAVA → OK"
else
    echo "[TLauncher] Пропущено — Java не найдена: $TLAUNCHER_JAVA"
fi

# ---- Системная Java (нужен sudo) ----
SYS_JAVA="/usr/lib/jvm/java-21-openjdk-amd64/bin/java"
SYS_REAL="${SYS_JAVA}.real"

if [ -f "$SYS_JAVA" ]; then
    echo "[System] Обновляю системную обёртку (потребуется пароль)..."
    sudo bash -c "
if [ ! -f '$SYS_REAL' ]; then
    mv '$SYS_JAVA' '$SYS_REAL'
fi
cat > '$SYS_JAVA' << EOF
#!/bin/bash
export LD_PRELOAD=$SO_PATH
exec $SYS_REAL \"\\\$@\" 2>>$LOG_PATH
EOF
chmod +x '$SYS_JAVA'
"
    echo "[System] Обёртка: $SYS_JAVA → OK"
else
    echo "[System] Пропущено — Java не найдена: $SYS_JAVA"
fi

echo "=== Готово ==="
echo "Лог Toolscreen: $LOG_PATH"
echo "Лог установки:  $SETUP_LOG"