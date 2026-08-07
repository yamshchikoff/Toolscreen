#include "resize_thread.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <queue>
#include <thread>

namespace ResizeThread {

namespace {

// ---- Состояние потока ----
std::thread g_thread;
std::mutex g_mutex;
std::condition_variable g_cv;
std::queue<ResizeJob> g_queue;
std::atomic<bool> g_stop{false};
std::atomic<int> g_processedCount{0};

// Собственное Display-соединение (открывается внутри потока)
Display* g_dpy = nullptr;

// ---- Логирование ----
void RT_LOG(const char* fmt, ...) {
    FILE* f = fopen("/home/user/toolscreen.log", "a");
    if (f) {
        va_list va;
        va_start(va, fmt);
        vfprintf(f, fmt, va);
        va_end(va);
        fflush(f);
        fclose(f);
    }
}

// ---- X11 error handler (не абортит процесс) ----
// XSetErrorHandler — процесс-глобален. Сохраняем предыдущий обработчик
// и пробрасываем ему ошибки не с нашего Display-соединения.
int (*g_prevErrorHandler)(Display*, XErrorEvent*) = nullptr;

int ResizeThreadErrorHandler(Display* dpy, XErrorEvent* ev) {
    // Ошибки с НАШЕГО соединения (g_dpy) — логируем и подавляем.
    // Ошибки с других соединений (основной GLFW Display) — пробрасываем
    // предыдущему обработчику, если он есть и g_dpy ещё жив.
    if (!g_dpy || dpy != g_dpy) {
        if (g_prevErrorHandler) return g_prevErrorHandler(dpy, ev);
        return 0;
    }

    char buf[256];
    XGetErrorText(ev->display, ev->error_code, buf, sizeof(buf));
    RT_LOG("[ResizeThread] X11 error: %s (opcode=%d, resource=0x%lx)\n",
           buf, ev->request_code, ev->resourceid);
    return 0;
}

// ---- Рабочий цикл потока ----
void ThreadLoop() {
    RT_LOG("[ResizeThread] thread started, opening Display\n");

    g_dpy = XOpenDisplay(nullptr);
    if (!g_dpy) {
        RT_LOG("[ResizeThread] FATAL: XOpenDisplay failed\n");
        return;
    }
    g_prevErrorHandler = XSetErrorHandler(ResizeThreadErrorHandler);
    RT_LOG("[ResizeThread] Display opened: %p\n", static_cast<void*>(g_dpy));

    while (!g_stop.load()) {
        std::unique_lock<std::mutex> lock(g_mutex);
        g_cv.wait(lock, []() {
            return !g_queue.empty() || g_stop.load();
        });

        if (g_stop.load() && g_queue.empty()) {
            break;
        }

        // Drain всей очереди под локом
        while (!g_queue.empty()) {
            ResizeJob job = g_queue.front();
            g_queue.pop();
            lock.unlock();

            RT_LOG("[ResizeThread] XConfigureWindow(win=0x%lx, %dx%d)\n",
                   job.window, job.width, job.height);

            XWindowChanges changes;
            changes.width = job.width;
            changes.height = job.height;
            XConfigureWindow(g_dpy, job.window, CWWidth | CWHeight, &changes);
            XFlush(g_dpy);

            RT_LOG("[ResizeThread] XConfigureWindow done\n");

            g_processedCount.fetch_add(1);
            lock.lock();
        }
    }

    RT_LOG("[ResizeThread] thread stopping, closing Display\n");
    if (g_dpy) {
        XCloseDisplay(g_dpy);
        g_dpy = nullptr;
    }
    RT_LOG("[ResizeThread] thread stopped\n");
}

}  // namespace

void Start() {
    if (g_thread.joinable()) {
        RT_LOG("[ResizeThread] Start: already running\n");
        return;
    }

    g_stop.store(false);
    g_processedCount.store(0);
    g_thread = std::thread(ThreadLoop);
    RT_LOG("[ResizeThread] Start: thread launched\n");
}

void Enqueue(Window win, int width, int height) {
    if (!g_thread.joinable()) {
        RT_LOG("[ResizeThread] Enqueue: thread not running, ignored\n");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_queue.push({win, width, height});
    }
    g_cv.notify_one();

    RT_LOG("[ResizeThread] Enqueue: win=0x%lx, %dx%d (queue size ~%zu)\n",
           win, width, height, g_queue.size());
}

void Stop() {
    RT_LOG("[ResizeThread] Stop: signalling\n");
    g_stop.store(true);
    g_cv.notify_one();

    if (g_thread.joinable()) {
        g_thread.join();
    }
    RT_LOG("[ResizeThread] Stop: joined\n");
}

int GetProcessedCount() {
    return g_processedCount.load();
}

}  // namespace ResizeThread
