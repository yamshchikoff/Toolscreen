#pragma once

#include <X11/Xlib.h>
#include <cstdint>

namespace ResizeThread {

struct ResizeJob {
    Window window;
    int width;
    int height;
};

// Запустить поток со своим Display-соединением.
void Start();

// Положить запрос в очередь. Потокобезопасно.
void Enqueue(Window win, int width, int height);

// Остановить поток и закрыть Display-соединение.
void Stop();

// Для тестов: количество обработанных запросов.
int GetProcessedCount();

}  // namespace ResizeThread
