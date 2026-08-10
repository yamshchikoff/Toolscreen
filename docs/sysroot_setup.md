# Восстановление sysroot для сборки

Проект использует `/tmp/sysroot` для заголовочных файлов и библиотек (когда системные -dev пакеты не установлены).

**Важно**: `/tmp` чистится при перезагрузке. Sysroot нужно восстанавливать после каждого ребута.

## Восстановление

```bash
# 1. Создать директорию и распаковать -dev пакеты (заголовки + .so-симлинки)
mkdir -p /tmp/sysroot
for deb in /home/user/Toolscreen/lib*-dev*.deb; do
    dpkg-deb -x "$deb" /tmp/sysroot
done

# 2. Скопировать реальные библиотеки из системы (симлинки из .deb ведут в никуда)
find /tmp/sysroot -type l -! -exec test -e {} \; -print | while read symlink; do
    target=$(readlink "$symlink")
    system_file="/usr/lib/x86_64-linux-gnu/$target"
    if [ -f "$system_file" ]; then
        cp -L "$system_file" "/tmp/sysroot/usr/lib/x86_64-linux-gnu/$target"
        echo "fixed: $target"
    fi
done
```

## Сборка

```bash
TOOLSCREEN_SYSROOT=/tmp/sysroot cmake --build out/build/linux-test --parallel $(nproc)
```

Для включения ImGui: добавить `-DTOOLSCREEN_ENABLE_IMGUI` при первой конфигурации cmake.
