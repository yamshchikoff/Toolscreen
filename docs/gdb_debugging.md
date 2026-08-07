# Отладка Minecraft при помощи GDB

> **Инструмент**: [`gdbwrap`](gdbwrap_usage.md) — персистентная GDB-сессия через файлы.

## Главное правило

**Отладка через GDB означает действия посредством GDB, а не написание кода.** Если программа падает — нужно аттачиться, ставить брикпинты, смотреть состояние. Не комментировать код чтобы «проверить гипотезу». Все проверки — через GDB.

**Доказательством вызова функции является только прямое наблюдение через GDB** (breakpoint hit, dprintf hit count, tracepoint). Логи, printf и любые косвенные признаки доказательством не считаются.

## Протокол сессии

1. Попросить пользователя зайти в мир
2. `gdbwrap start gdb --pid <PID>` — аттач (загружает символы, до 60 секунд)
3. `cmd "handle SIGSEGV nostop pass"` — JVM-сигналы не должны останавливать
4. `cmd "handle SIGSTOP nostop pass"` — и SIGSTOP тоже
5. Отладка: `cmd "bt"`, `cmd "info threads"`, `cmd "break ..."`, `cmd "continue"`
6. Для прерывания: `kill -INT <gdb-pid>` — возвращает gdb на промпт
7. `gdbwrap stop` — завершить

## Инжект через gdbwrap

Инжектор `inject.sh` работает через batch-gdb. gdbwrap тоже умеет инжектить — нужен `scheduler-locking on` и предварительный SIGINT:

```bash
# 1. Прервать выполнение (все потоки остановлены)
kill -INT $(pgrep -f "/usr/bin/gdb")

# 2. Заблокировать планировщик
gdbwrap cmd "set scheduler-locking on"

# 3. Вызвать dlopen
gdbwrap cmd "call (void*)dlopen(\"/home/user/Toolscreen/out/build/linux-test/bin/libtoolscreen.so\", 2)"
# → $1 = (void *) 0x...

# 4. Проверить ошибку
gdbwrap cmd "call (char*)dlerror()"
# → $2 = 0x0  (нет ошибки)

# 5. Разблокировать
gdbwrap cmd "set scheduler-locking off"
```

## Успешная отладка XNextEvent-хука

**Дата**: 2026-07-24

### Что сделали

1. Заинжектили `libtoolscreen.so` через gdbwrap (scheduler-locking + SIGINT + call dlopen)
2. Поставили брикпинт на `DetourXNextEvent` (адрес = base + 0x3aac10)
3. `continue` — игра работает
4. Нажали клавиши — **брикпинт сработал** — GLFW → XNextEvent → DetourXNextEvent
5. Пошагали через DetourXNextEvent: `endbr64` → `mov rax,[rip]` → `test rax` → `jmp *rax`
6. Прыгнули в трамплин: `endbr64` → `push rbp` → `jmp XNextEvent+5`
7. Прочитали XEvent: `x/1xw $rsi` → тип 7 = EnterNotify (мышь)
8. **Хук работает корректно**: трамплин прыгает на правильный адрес (XNextEvent+5), дизассемблер X86InsnMinCover вернул 5, всё чисто

### Ключевые находки

- **Архитектура GDB**: без `set architecture i386:x86-64` gdb определяет архитектуру как i386 и не может читать 64-битную память. gdbwrap делает это автоматически при старте.
- **Символы**: `sharedlibrary` грузит символы всех .so (долго, но нужно для `call dlopen`)
- **scheduler-locking**: без него `call dlopen` срывается сигналами из других потоков JVM
- **Pass-through хук не крашит**: DetourXNextEvent с чистым `jmp *%rax` (без чтения XEvent) — игра работает стабильно
- **Трамплин правильный**: `X86InsnMinCover` вернул 5 (endbr64 4 + push 1), трамплин прыгает на XNextEvent+5 = `mov rsp,rbp`

## Проверка inline-хуков после установки

После инжекта и установки inline-хука нужно на паузе проверить корректность:

1. **Патч по адресу**: `x/1i <target>` — первый байт должен быть `jmp <detour>`
2. **Куда прыгает**: сравнить адрес после `jmp` с ожидаемым адресом detour'а
3. **Трамплин**: прочитать `g_real<Fn>` (или переменную с адресом трамплина), дизассемблировать — должен содержать сохранённые байты + `jmp target+backupLen`
4. **Куда прыгает трамплин**: `x/1i <trampoline+backupLen>` — адрес должен быть `target + backupLen`
5. **Дистанции**: проверить что все `rel32` влезают в ±2GB (трамплин рядом с target'ом, detour рядом с target'ом или bridge)

Пример проверки (сегодня):
```
(gdb) x/1i XNextEvent
   jmp 0x7fa7c0daac10         ; правильно → DetourXNextEvent
(gdb) x/1gx &g_realXNextEvent
   0x7fa8e8033000              ; адрес трамплина
(gdb) x/3i 0x7fa8e8033000
   endbr64; push rbp; jmp 0x7fa930cf23b5  ; ← БАГ: прыжок на +4GB!
```

## Особенности

- JVM генерит внутренние SIGSEGV и SIGSTOP — `handle SIGSEGV nostop pass` + `handle SIGSTOP nostop pass`
- `info threads` показывает 60+ потоков, большинство в `futex_wait`
- JIT-скомпилированный код без символов — `bt` показывает `??`
- `continue` зависает — игра работает, `cmd` ждёт `(gdb)` до следующего сигнала
- Долгий аттач: 10-20 секунд, `sharedlibrary` ещё до 30 секунд
