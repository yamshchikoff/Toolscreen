# Баги

## BUG-001: gdb прерывает dlopen при инжекте

**Симптомы**: при запуске `sudo ./scripts/inject.sh` gdb выдаёт:
```
warning: Could not load shared library symbols for /tmp/jna-3599307/...
(___dlopen) will be abandoned.
[Switching to Thread ...]
(___dlopen) will be abandoned.
$1 = 0x0
```

`$1 = 0x0` — результат `dlerror()`, а не `dlopen()`. Библиотека `libtoolscreen.so` либо не загружена, либо загружена не полностью.

**Следствие**: GL-хуки и X11-хуки могут не установиться или установиться в битом состоянии → неопределённое поведение, краши.

**Вероятная причина**: gdb теряет вызов `dlopen` при переключении между потоками JVM. Потоки Minecraft'а активны во время инжекта.

**Исправление**: добавлен `-ex "set scheduler-locking on"` перед вызовами dlopen. Запрещает gdb переключать потоки во время вызова — dlopen больше не обрывается.

**Дата обнаружения**: 2026-07-24
**Дата исправления**: 2026-07-24

## BUG-002: thread_local вызывает __tls_get_addr — ломает стек X11

**Симптомы**: `DetourXNextEvent` с `thread_local XEvent* tls_pendingEvent` вызывает краш `SIGSEGV SEGV_MAPERR` по адресу `XNextEvent + 5 + 256MB`. Краш происходит при возврате из трамплина — битый return address на стеке.

**Причина**: `thread_local` в разделяемой библиотеке использует "global-dynamic" TLS-модель. Каждый доступ к переменной вызывает `__tls_get_addr()` — функцию, которая создаёт стек-фрейм. `DetourXNextEvent` выполняется внутри `XLockDisplay` — вызов `__tls_get_addr` ломает стек, по которому ходит X11.

Ассемблер (из object-файла):
```asm
DetourXNextEvent:
    endbr64
    push   %r12                     ; сохраняем регистры
    mov    g_realXNextEvent(%rip),%r12
    push   %rbp
    push   %rbx
    test   %r12,%r12
    je     .Lerror
    ...
    lea    tls_pendingEvent(%rip),%rdi
    call   __tls_get_addr           ; ← ВОТ ПРОБЛЕМА! Стек-фрейм внутри XLockDisplay
    mov    %rbx,(%rax)              ; tls_pendingEvent = event_return
    ...
    jmp    *%rax                    ; tail call правильный, но стек уже испорчен
```

**Решение**: заменить `thread_local` на `static` (XNextEvent всегда под XLockDisplay, один Display → один поток → thread-safety не нужна).

**Урок**: в inline-хуках на X11-функции нельзя использовать `thread_local` — он вызывает `__tls_get_addr`. Аналогично нельзя вызывать любые функции (включая `fopen`/`fprintf` для логов). Только pure computation и tail call.

**Дата обнаружения**: 2026-07-24

## BUG-003: краш даже с чистым asm (без call, без push, без thread_local)

**Симптомы**: после исправления BUG-002 (static вместо thread_local, musttail) ассемблер `DetourXNextEvent` чистый:
```asm
mov    g_realXNextEvent(%rip),%rax
test   %rax,%rax / je
test   %rsi,%rsi / je
mov    %rsi,s_pendingEvent(%rip)
jmp    *%rax              ; чистый tail call
```
Ни одного `push`, ни одного `call` в теле функции. Но игра всё равно крашится с `SIGSEGV SEGV_MAPERR` по адресу `libX11_base + X + 0x3b5` (offset 0x3b5 = XNextEvent+5 в пределах страницы, но битый номер страницы).

**Гипотезы**:
1. `mov %rsi, s_pendingEvent(%rip)` — запись в глобальную переменную внутри XLockDisplay вызывает проблему
2. Чтение `s_pendingEvent` в `hk_glXSwapBuffers` (drain) — разыменование XEvent* на рендер-потоке
3. Трамплин прыгает неправильно независимо от нашего кода

**Что нужно проверить** (бинарный поиск):
- Отключить drain (не читать s_pendingEvent в hk_glXSwapBuffers) → краш?
- Отключить запись в s_pendingEvent (pass-through) → не крашит? (уже проверено — pass-through работает)

**Дата обнаружения**: 2026-07-24
