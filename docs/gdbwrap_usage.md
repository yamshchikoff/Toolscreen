# Использование gdbwrap

## Запуск

```bash
# Запустить сервер (блокирующий — в фоне):
python3 scripts/gdbwrap start gdb <программа> &

# Ждать появления READY:
# В логе появится "READY <pid>"
```

## Команды

```bash
# Выполнить команду GDB и получить вывод:
python3 scripts/gdbwrap cmd "<gdb-команда>"

# Остановить сервер:
python3 scripts/gdbwrap stop
```

## Пример сессии

```bash
# Терминал 1 (запуск сервера):
$ python3 scripts/gdbwrap start gdb /tmp/test_prog &
READY 12345

# Терминал 2 (команды):
$ python3 scripts/gdbwrap cmd "break main"
Breakpoint 1 at 0x1173: file /tmp/test_prog.c, line 7.

$ python3 scripts/gdbwrap cmd "run"
Starting program: /tmp/test_prog
Breakpoint 1, main () at test_prog.c:7

$ python3 scripts/gdbwrap cmd "step"
8    int z = add(x, y);

$ python3 scripts/gdbwrap cmd "print x"
$1 = 10

$ python3 scripts/gdbwrap cmd "info registers"
rax  0x555555555167  ...

$ python3 scripts/gdbwrap cmd "continue"
x=10 y=20 z=30
[Inferior 1 exited normally]

$ python3 scripts/gdbwrap stop
```

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
