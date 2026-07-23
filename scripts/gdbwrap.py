#!/usr/bin/env python3
"""Обёртка над GDB для интерактивного управления через файлы.

Запуск:
  gdbwrap.py attach <pid>          — аттачится и ждёт команд
  gdbwrap.py cmd <command...>      — посылает команду и читает ответ
  gdbwrap.py detach                — detach и выход

Состояние хранится в /tmp/gdbwrap_* (PID gdb, FIFO).
"""

import os
import sys
import subprocess
import time
import signal

STATE_DIR = "/tmp/gdbwrap"
FIFO_IN = f"{STATE_DIR}/in"
FIFO_OUT = f"{STATE_DIR}/out"
PID_FILE = f"{STATE_DIR}/pid"
LOCK_FILE = f"{STATE_DIR}/lock"

def ensure_dirs():
    os.makedirs(STATE_DIR, exist_ok=True)

def attach(pid):
    ensure_dirs()
    # Убить предыдущий если есть
    detach()
    # Создать FIFO
    for f in [FIFO_IN, FIFO_OUT]:
        if os.path.exists(f):
            os.unlink(f)
        os.mkfifo(f)

    sudo_pass = "rabbit chess"

    # Запускаем gdb через sudo с паролем
    cmd = f"echo '{sudo_pass}' | sudo -S gdb --pid {pid} -q < {FIFO_IN} > {FIFO_OUT} 2>&1"
    proc = subprocess.Popen(cmd, shell=True, preexec_fn=os.setsid)

    with open(PID_FILE, 'w') as f:
        f.write(str(proc.pid))

    time.sleep(0.5)
    # Читаем начальный вывод
    print(read_output(timeout=2))
    print("GDB_READY")

def detach():
    try:
        cmd("detach", timeout=2)
        cmd("quit", timeout=2)
    except:
        pass
    # Убить процесс
    if os.path.exists(PID_FILE):
        with open(PID_FILE) as f:
            pid = int(f.read().strip())
        try:
            os.killpg(os.getpgid(pid), signal.SIGTERM)
        except:
            pass
    # Очистить
    for f in [FIFO_IN, FIFO_OUT, PID_FILE]:
        if os.path.exists(f):
            os.unlink(f)

def cmd(command, timeout=5):
    """Послать команду в GDB и прочитать ответ."""
    with open(FIFO_IN, 'w') as f:
        f.write(command + "\n")
        f.flush()
    return read_output(timeout)

def read_output(timeout=5):
    """Прочитать доступный вывод GDB."""
    import select
    result = []
    try:
        fd = os.open(FIFO_OUT, os.O_RDONLY | os.O_NONBLOCK)
        deadline = time.time() + timeout
        while time.time() < deadline:
            r, _, _ = select.select([fd], [], [], 0.1)
            if r:
                try:
                    data = os.read(fd, 65536)
                    if data:
                        result.append(data.decode('utf-8', errors='replace'))
                    else:
                        break
                except BlockingIOError:
                    break
            if result and b'\n' in result[-0].encode() if result else False:
                pass  # продолжать читать пока есть данные
        os.close(fd)
    except Exception as e:
        return f"READ_ERROR: {e}"
    return ''.join(result)

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: gdbwrap.py attach <pid> | cmd <command> | detach")
        sys.exit(1)

    action = sys.argv[1]

    if action == 'attach':
        attach(sys.argv[2])
    elif action == 'cmd':
        print(cmd(' '.join(sys.argv[2:])))
    elif action == 'detach':
        detach()
    else:
        print(f"Unknown action: {action}")
