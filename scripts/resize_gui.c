// resize_gui: GUI-кнопка для ресайза окна Minecraft
// Сборка: gcc -o /tmp/resize_gui scripts/resize_gui.c -lX11
// Запуск: /tmp/resize_gui <window_id>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    Display *dpy;
    Window mcWin;       // окно Minecraft
    Window guiWin;      // наше окно
    Window btnResize;   // кнопка "Resize 800x600"
    Window btnRestore;  // кнопка "Restore 925x530"
    GC gc;
    int btnResizeX, btnResizeY, btnResizeW, btnResizeH;
    int btnRestoreX, btnRestoreY, btnRestoreW, btnRestoreH;
    int guiW, guiH;
    int mcOrigW, mcOrigH;  // исходный размер Minecraft
    int clickCount;
} App;

static void drawButton(App *a, Window btn, int x, int y, int w, int h, const char *label, int pressed) {
    unsigned long bg = pressed ? 0x888888 : 0xCCCCCC;
    unsigned long fg = 0x000000;
    XSetForeground(a->dpy, a->gc, bg);
    XFillRectangle(a->dpy, btn, a->gc, 0, 0, w, h);
    XSetForeground(a->dpy, a->gc, 0x555555);
    XDrawRectangle(a->dpy, btn, a->gc, 0, 0, w - 1, h - 1);
    XSetForeground(a->dpy, a->gc, fg);
    int tw = XTextWidth(XQueryFont(a->dpy, XGContextFromGC(a->gc)), label, strlen(label));
    int th = 14;
    XDrawString(a->dpy, btn, a->gc, (w - tw) / 2, (h + th) / 2, label, strlen(label));
}

static void resizeMC(App *a, int w, int h) {
    printf("[resize_gui] XResizeWindow(0x%lx, %d, %d)\n", a->mcWin, w, h);
    XResizeWindow(a->dpy, a->mcWin, w, h);
    XFlush(a->dpy);

    // Проверим что получилось
    usleep(100000);
    XWindowAttributes attrs;
    if (XGetWindowAttributes(a->dpy, a->mcWin, &attrs)) {
        printf("[resize_gui] now: %dx%d\n", attrs.width, attrs.height);
    } else {
        printf("[resize_gui] XGetWindowAttributes failed!\n");
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <minecraft_window_id>\n", argv[0]);
        return 1;
    }

    App a;
    memset(&a, 0, sizeof(a));
    a.mcWin = (Window)strtoul(argv[1], NULL, 16);
    a.clickCount = 0;

    a.dpy = XOpenDisplay(":1");
    if (!a.dpy) { perror("XOpenDisplay"); return 1; }

    // Узнать исходный размер Minecraft
    XWindowAttributes attrs;
    if (XGetWindowAttributes(a.dpy, a.mcWin, &attrs)) {
        a.mcOrigW = attrs.width;
        a.mcOrigH = attrs.height;
        printf("[resize_gui] Minecraft window 0x%lx, current size %dx%d\n",
               a.mcWin, a.mcOrigW, a.mcOrigH);
    } else {
        fprintf(stderr, "[resize_gui] ERROR: cannot get Minecraft window attributes!\n");
        a.mcOrigW = 925;
        a.mcOrigH = 530;
    }

    int screen = DefaultScreen(a.dpy);
    Window root = RootWindow(a.dpy, screen);

    a.guiW = 280;
    a.guiH = 120;

    a.guiWin = XCreateSimpleWindow(a.dpy, root, 100, 100, a.guiW, a.guiH,
                                    1, BlackPixel(a.dpy, screen), 0xEEEEEE);

    // Кнопка Resize
    a.btnResizeX = 20;
    a.btnResizeY = 25;
    a.btnResizeW = 240;
    a.btnResizeH = 30;
    a.btnResize = XCreateSimpleWindow(a.dpy, a.guiWin,
                                       a.btnResizeX, a.btnResizeY,
                                       a.btnResizeW, a.btnResizeH,
                                       1, BlackPixel(a.dpy, screen), 0xCCCCCC);

    // Кнопка Restore
    a.btnRestoreX = 20;
    a.btnRestoreY = 65;
    a.btnRestoreW = 240;
    a.btnRestoreH = 30;
    a.btnRestore = XCreateSimpleWindow(a.dpy, a.guiWin,
                                        a.btnRestoreX, a.btnRestoreY,
                                        a.btnRestoreW, a.btnRestoreH,
                                        1, BlackPixel(a.dpy, screen), 0xCCCCCC);

    XSelectInput(a.dpy, a.guiWin, ExposureMask | KeyPressMask);
    XSelectInput(a.dpy, a.btnResize,
                 ExposureMask | ButtonPressMask | ButtonReleaseMask |
                 EnterWindowMask | LeaveWindowMask);
    XSelectInput(a.dpy, a.btnRestore,
                 ExposureMask | ButtonPressMask | ButtonReleaseMask |
                 EnterWindowMask | LeaveWindowMask);

    XStoreName(a.dpy, a.guiWin, "Minecraft Resizer");
    XMapWindow(a.dpy, a.guiWin);
    XMapWindow(a.dpy, a.btnResize);
    XMapWindow(a.dpy, a.btnRestore);

    a.gc = XCreateGC(a.dpy, a.guiWin, 0, NULL);

    printf("[resize_gui] GUI ready. Press buttons to resize Minecraft.\n");
    printf("[resize_gui] q = quit\n");

    int resizeBtnPressed = 0;
    int restoreBtnPressed = 0;

    while (1) {
        XEvent ev;
        XNextEvent(a.dpy, &ev);

        if (ev.type == KeyPress) {
            char buf[2];
            if (XLookupString(&ev.xkey, buf, sizeof(buf), NULL, NULL) == 1 && buf[0] == 'q') {
                break;
            }
        }

        if (ev.type == Expose) {
            if (ev.xexpose.window == a.btnResize) {
                drawButton(&a, a.btnResize, a.btnResizeX, a.btnResizeY,
                          a.btnResizeW, a.btnResizeH,
                          resizeBtnPressed ? "> Resizing..." : "Resize to 800x600",
                          resizeBtnPressed);
            } else if (ev.xexpose.window == a.btnRestore) {
                char label[64];
                snprintf(label, sizeof(label), "Restore %dx%d", a.mcOrigW, a.mcOrigH);
                drawButton(&a, a.btnRestore, a.btnRestoreX, a.btnRestoreY,
                          a.btnRestoreW, a.btnRestoreH,
                          restoreBtnPressed ? "> Restoring..." : label,
                          restoreBtnPressed);
            }
        }

        if (ev.type == ButtonPress && ev.xbutton.button == 1) {
            if (ev.xbutton.window == a.btnResize) {
                resizeBtnPressed = 1;
                drawButton(&a, a.btnResize, a.btnResizeX, a.btnResizeY,
                          a.btnResizeW, a.btnResizeH, "> Resizing...", 1);
                XFlush(a.dpy);
                resizeMC(&a, 800, 600);
            } else if (ev.xbutton.window == a.btnRestore) {
                restoreBtnPressed = 1;
                char label[64];
                snprintf(label, sizeof(label), "> Restoring...");
                drawButton(&a, a.btnRestore, a.btnRestoreX, a.btnRestoreY,
                          a.btnRestoreW, a.btnRestoreH, label, 1);
                XFlush(a.dpy);
                resizeMC(&a, a.mcOrigW, a.mcOrigH);
            }
        }

        if (ev.type == ButtonRelease && ev.xbutton.button == 1) {
            if (ev.xbutton.window == a.btnResize) {
                resizeBtnPressed = 0;
                drawButton(&a, a.btnResize, a.btnResizeX, a.btnResizeY,
                          a.btnResizeW, a.btnResizeH, "Resize to 800x600", 0);
            } else if (ev.xbutton.window == a.btnRestore) {
                restoreBtnPressed = 0;
                char label[64];
                snprintf(label, sizeof(label), "Restore %dx%d", a.mcOrigW, a.mcOrigH);
                drawButton(&a, a.btnRestore, a.btnRestoreX, a.btnRestoreY,
                          a.btnRestoreW, a.btnRestoreH, label, 0);
            }
        }
    }

    XCloseDisplay(a.dpy);
    return 0;
}
