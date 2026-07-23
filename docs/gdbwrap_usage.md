# Использование gdbwrap

## Запуск

```bash
# Запустить сервер (блокирующий — в фоне):
python3 scripts/gdbwrap start gdb <программа> &

# Для процессов без sudo (если kernel.yama.ptrace_scope=0):
python3 scripts/gdbwrap start gdb --pid <PID> &

# Ждать READY (может занять до 20+ секунд для больших процессов):
cat /tmp/gdbwrap/ready  # или ждать в логе "READY"
```

## Команды

```bash
# Выполнить команду GDB и получить вывод:
python3 scripts/gdbwrap cmd "<gdb-команда>"

# Прервать выполнение (Ctrl+C):
kill -INT $(ps aux | grep "/usr/bin/gdb" | grep -v grep | awk '{print $2}')

# Остановить сервер:
python3 scripts/gdbwrap stop
```

## Пример: отладка простой программы

```bash
$ python3 scripts/gdbwrap start gdb /tmp/test_prog &
READY 12345

$ python3 scripts/gdbwrap cmd "break main"
Breakpoint 1 at 0x1173

$ python3 scripts/gdbwrap cmd "run"
Breakpoint 1, main () at test_prog.c:7

$ python3 scripts/gdbwrap cmd "step"
8    int z = add(x, y);

$ python3 scripts/gdbwrap cmd "print x"
$1 = 10

$ python3 scripts/gdbwrap cmd "continue"
x=10 y=20 z=30
[Inferior 1 exited normally]

$ python3 scripts/gdbwrap stop
```

## Пример: отладка Minecraft (без sudo)

```bash
# Проверить ptrace_scope (должен быть 0)
$ cat /proc/sys/kernel/yama/ptrace_scope
0

# Аттач (PID можно найти через pgrep)
$ python3 scripts/gdbwrap start gdb --pid $(pgrep -f runtime/jre-legacy.*java) &
# Ждать ~15-20 секунд — GDB загружает 60+ потоков
$ cat /tmp/gdbwrap/ready  # проверка готовности

# ПЕРВЫМ ДЕЛОМ: не останавливаться на JVM-сигналах
$ python3 scripts/gdbwrap cmd "handle SIGSEGV nostop pass"
Signal        Stop    Pass to program Description
SIGSEGV       No      Yes             Segmentation fault

# Команды
$ python3 scripts/gdbwrap cmd "info threads"
  1    Thread ... "java"  __futex_abstimed_wait_common64
  2    Thread ... "java"  __futex_abstimed_wait_common64
  ...
* 16   Thread ... "java"  signalHandler

$ python3 scripts/gdbwrap cmd "bt"
#0  0x... in ?? ()       <- JIT-код без символов

$ python3 scripts/gdbwrap cmd "info proc mappings"
  ...                      <- все регионы памяти

$ python3 scripts/gdbwrap cmd "continue"
Continuing.                <- игра разморожена, cmd зависнет до следующего сигнала/брикпинта

# Прервать игру (Ctrl+C):
$ kill -INT $(ps aux | grep "/usr/bin/gdb" | grep -v grep | awk '{print $2}')
# GDB возвращается на (gdb), можно снова командовать

$ python3 scripts/gdbwrap stop
```

## Инжект .so через gdbwrap (`call dlopen`)

```bash
# 1. Остановить все потоки
$ kill -INT $(pgrep -f "/usr/bin/gdb")

# 2. Заблокировать планировщик
$ python3 scripts/gdbwrap cmd "set scheduler-locking on"

# 3. Инжект
$ python3 scripts/gdbwrap cmd "call (void*)dlopen(\"/путь/к/libtoolscreen.so\", 2)"
$1 = (void *) 0x...   # ненулевой = успех

# 4. Проверить
$ python3 scripts/gdbwrap cmd "call (char*)dlerror()"
$2 = 0x0              # нет ошибки

# 5. Разблокировать
$ python3 scripts/gdbwrap cmd "set scheduler-locking off"
```

## Особенности работы с Minecraft

- **Долгий аттач**: 60+ потоков, paging — READY появляется через 10-20 секунд
- **SIGSEGV — норма**: JVM генерит внутренние SIGSEGV (`Parker::park`, `signalHandler`). Без `handle SIGSEGV nostop pass` gdb будет останавливаться на каждом, играть невозможно. С этой командой — SIGSEGV молча передаются JVM, игра работает.
- **Первая команда после аттача**: `python3 scripts/gdbwrap cmd "handle SIGSEGV nostop pass"`
- **Нет символов**: JVM-библиотеки stripped, JIT-код без отладочной информации — `bt` показывает `??`. Но функции JVM (`Parker::park`) видны
- **`continue` зависает**: игра работает, `cmd` ждёт `(gdb)` который не появится до сигнала. Прервать через `kill -INT`
- **`-iex="set pagination off"`**: gdbwrap передаёт этот флаг автоматом, иначе вывод из 60+ потоков уходит в paging

## Как это работает

- `start` запускает GDB через pexpect и входит в серверный цикл
- Сервер создаёт файл `/tmp/gdbwrap/ready` когда готов принимать команды
- `cmd` ждёт `ready`, пишет команду в `/tmp/gdbwrap/cmd`, ждёт ответ в `/tmp/gdbwrap/out`
- Сервер читает `cmd`, отправляет в GDB, ловит `(gdb)`, пишет ответ в `out`
- `stop` создаёт `/tmp/gdbwrap/stop`, сервер завершает GDB

## Файлы

| Файл | Назначение |
|------|-----------|
| `/tmp/gdbwrap/ready` | Сервер готов |
| `/tmp/gdbwrap/cmd` | Очередная команда |
| `/tmp/gdbwrap/out` | Вывод последней команды |
| `/tmp/gdbwrap/stop` | Сигнал остановки |

## Требования

Только `pexpect` (`pip install pexpect`).
