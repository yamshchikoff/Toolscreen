// resize_gui: GUI-кнопка для ресайза окна Minecraft
// Находит окно Minecraft автоматически — аргументы не нужны.
// Сборка: gcc -o /tmp/resize_gui scripts/resize_gui.c -lX11
// Запуск: /tmp/resize_gui
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    Display *dpy;
    Window mcWin;
    Window guiWin;
    Window btnResize;
    Window btnRestore;
    GC gc;
    int btnResizeX, btnResizeY, btnResizeW, btnResizeH;
    int btnRestoreX, btnRestoreY, btnRestoreW, btnRestoreH;
    int guiW, guiH;
    int mcOrigW, mcOrigH;
    char statusText[256];
} App;

// ---- Поиск окна Minecraft ----
// Используем _NET_CLIENT_LIST — только топлевел-окна, управляемые WM.
// Рекурсивный обход XQueryTree может найти дочерние GLX-окна,
// на которых XResizeWindow не работает (BadWindow).

static Window findMinecraftWindow(Display *dpy, Window root, char **nameOut) {
    Atom netClientList = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    Atom netWmName    = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom wmClass       = XInternAtom(dpy, "WM_CLASS", False);

    Atom type; int fmt; unsigned long nitems, after; unsigned char *prop = NULL;

    // Получаем список топлевел-окон от WM
    if (XGetWindowProperty(dpy, root, netClientList, 0, ~0L, False,
                           XA_WINDOW, &type, &fmt, &nitems, &after, &prop) != Success || !prop)
        return 0;

    Window *clients = (Window*)prop;
    Window found = 0;

    for (unsigned long i = 0; i < nitems; i++) {
        Window w = clients[i];

        // Проверяем WM_CLASS на "Minecraft"
        Atom ct; int cf; unsigned long cn, ca; unsigned char *cp = NULL;
        if (XGetWindowProperty(dpy, w, wmClass, 0, 256, False,
                               AnyPropertyType, &ct, &cf, &cn, &ca, &cp) == Success && cp) {
            if (strstr((char*)cp, "Minecraft")) {
                found = w;
                // Читаем имя для статуса
                Atom nt; int nf; unsigned long nn, na; unsigned char *np = NULL;
                if (XGetWindowProperty(dpy, w, netWmName, 0, 256, False,
                                       AnyPropertyType, &nt, &nf, &nn, &na, &np) == Success && np) {
                    if (nameOut) *nameOut = strdup((char*)np);
                    XFree(np);
                }
                XFree(cp);
                break;
            }
            XFree(cp);
        }
    }
    XFree(prop);
    return found;
}

// ---- Отрисовка ----

static unsigned long rgb(Display *dpy, int r, int g, int b) {
    Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));
    XColor c;
    c.red = r * 257;
    c.green = g * 257;
    c.blue = b * 257;
    c.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(dpy, cmap, &c);
    return c.pixel;
}

static void drawButton(App *a, Window btn, int w, int h, const char *label, int pressed) {
    Colormap cmap = DefaultColormap(a->dpy, DefaultScreen(a->dpy));
    unsigned long bg = pressed ? rgb(a->dpy, 0x66, 0x66, 0x66) : rgb(a->dpy, 0xCC, 0xCC, 0xCC);
    unsigned long fg = rgb(a->dpy, 0x00, 0x00, 0x00);
    unsigned long border = rgb(a->dpy, 0x55, 0x55, 0x55);

    XSetWindowBackground(a->dpy, btn, bg);
    XClearWindow(a->dpy, btn);

    XSetForeground(a->dpy, a->gc, border);
    XDrawRectangle(a->dpy, btn, a->gc, 0, 0, w - 1, h - 1);

    XSetForeground(a->dpy, a->gc, fg);
    XFontStruct *font = XQueryFont(a->dpy, XGContextFromGC(a->gc));
    int tw = XTextWidth(font, label, strlen(label));
    int th = font->ascent;
    XDrawString(a->dpy, btn, a->gc, (w - tw) / 2, (h + th) / 2, label, strlen(label));
}

static void drawStatus(App *a) {
    XSetWindowBackground(a->dpy, a->guiWin, rgb(a->dpy, 0xEE, 0xEE, 0xEE));
    XClearWindow(a->dpy, a->guiWin);
    XSetForeground(a->dpy, a->gc, rgb(a->dpy, 0x33, 0x33, 0x33));
    XFontStruct *font = XQueryFont(a->dpy, XGContextFromGC(a->gc));
    XDrawString(a->dpy, a->guiWin, a->gc, 15, 20 + font->ascent,
                a->statusText, strlen(a->statusText));
}

// ---- Ресайз ----

static void resizeMC(App *a, int w, int h) {
    printf("[resize_gui] XResizeWindow(0x%lx, %d, %d)\n", a->mcWin, w, h);
    XResizeWindow(a->dpy, a->mcWin, w, h);
    XFlush(a->dpy);

    usleep(100000);
    XWindowAttributes attrs;
    if (XGetWindowAttributes(a->dpy, a->mcWin, &attrs)) {
        printf("[resize_gui] now: %dx%d\n", attrs.width, attrs.height);
        snprintf(a->statusText, sizeof(a->statusText),
                 "Minecraft 0x%lx — resized to %dx%d  (was %dx%d)",
                 a->mcWin, attrs.width, attrs.height, a->mcOrigW, a->mcOrigH);
    }
}

// ---- Main ----

int main(void) {
    App a;
    memset(&a, 0, sizeof(a));

    a.dpy = XOpenDisplay(":1");
    if (!a.dpy) {
        a.dpy = XOpenDisplay(":0");
        if (!a.dpy) {
            fprintf(stderr, "Cannot open X display :0 or :1\n");
            return 1;
        }
    }

    int screen = DefaultScreen(a.dpy);
    Window root = RootWindow(a.dpy, screen);

    // Ищем окно Minecraft
    char *mcName = NULL;
    a.mcWin = findMinecraftWindow(a.dpy, root, &mcName);

    if (!a.mcWin) {
        fprintf(stderr, "Minecraft window not found. Is Minecraft running?\n");
        XCloseDisplay(a.dpy);
        return 1;
    }

    printf("[resize_gui] Found Minecraft: 0x%lx \"%s\"\n", a.mcWin, mcName ? mcName : "");

    // Исходный размер
    XWindowAttributes attrs;
    if (XGetWindowAttributes(a.dpy, a.mcWin, &attrs)) {
        a.mcOrigW = attrs.width;
        a.mcOrigH = attrs.height;
    } else {
        a.mcOrigW = 925;
        a.mcOrigH = 530;
    }

    snprintf(a.statusText, sizeof(a.statusText),
             "Minecraft 0x%lx \"%s\" — %dx%d",
             a.mcWin, mcName ? mcName : "?", a.mcOrigW, a.mcOrigH);
    free(mcName);

    a.guiW = 330;
    a.guiH = 160;

    a.guiWin = XCreateSimpleWindow(a.dpy, root, 100, 100, a.guiW, a.guiH,
                                    1, BlackPixel(a.dpy, screen), 0xEEEEEE);

    // Кнопка Resize 800x600
    a.btnResizeX = 20;  a.btnResizeY = 35;
    a.btnResizeW = 290; a.btnResizeH = 32;
    a.btnResize = XCreateSimpleWindow(a.dpy, a.guiWin,
                                       a.btnResizeX, a.btnResizeY,
                                       a.btnResizeW, a.btnResizeH,
                                       1, BlackPixel(a.dpy, screen), 0xCCCCCC);

    // Кнопка Restore
    a.btnRestoreX = 20;  a.btnRestoreY = 78;
    a.btnRestoreW = 290; a.btnRestoreH = 32;
    a.btnRestore = XCreateSimpleWindow(a.dpy, a.guiWin,
                                        a.btnRestoreX, a.btnRestoreY,
                                        a.btnRestoreW, a.btnRestoreH,
                                        1, BlackPixel(a.dpy, screen), 0xCCCCCC);

    // Кнопка рефреш (пересканировать окна)
    Window btnRefresh;
    int btnRefreshX = 20, btnRefreshY = 121, btnRefreshW = 290, btnRefreshH = 28;
    btnRefresh = XCreateSimpleWindow(a.dpy, a.guiWin,
                                      btnRefreshX, btnRefreshY,
                                      btnRefreshW, btnRefreshH,
                                      1, BlackPixel(a.dpy, screen), 0xCCCCCC);

    XSelectInput(a.dpy, a.guiWin, ExposureMask | KeyPressMask);
    XSelectInput(a.dpy, a.btnResize, ExposureMask | ButtonPressMask | ButtonReleaseMask);
    XSelectInput(a.dpy, a.btnRestore, ExposureMask | ButtonPressMask | ButtonReleaseMask);
    XSelectInput(a.dpy, btnRefresh, ExposureMask | ButtonPressMask | ButtonReleaseMask);

    XStoreName(a.dpy, a.guiWin, "MC Resizer");
    XMapWindow(a.dpy, a.guiWin);
    XMapWindow(a.dpy, a.btnResize);
    XMapWindow(a.dpy, a.btnRestore);
    XMapWindow(a.dpy, btnRefresh);

    a.gc = XCreateGC(a.dpy, a.guiWin, 0, NULL);
    drawStatus(&a);

    int resizePressed = 0, restorePressed = 0, refreshPressed = 0;

    while (1) {
        XEvent ev;
        XNextEvent(a.dpy, &ev);

        if (ev.type == KeyPress) {
            char buf[2];
            if (XLookupString(&ev.xkey, buf, sizeof(buf), NULL, NULL) == 1) {
                if (buf[0] == 'q' || buf[0] == 27) break;
            }
        }

        if (ev.type == Expose) {
            if (ev.xexpose.window == a.guiWin) drawStatus(&a);
            else if (ev.xexpose.window == a.btnResize)
                drawButton(&a, a.btnResize, a.btnResizeW, a.btnResizeH,
                          resizePressed ? "> Resizing..." : "Resize to 800x600", resizePressed);
            else if (ev.xexpose.window == a.btnRestore) {
                char lbl[64];
                snprintf(lbl, sizeof(lbl), "Restore %dx%d", a.mcOrigW, a.mcOrigH);
                drawButton(&a, a.btnRestore, a.btnRestoreW, a.btnRestoreH,
                          restorePressed ? "> Restoring..." : lbl, restorePressed);
            } else if (ev.xexpose.window == btnRefresh)
                drawButton(&a, btnRefresh, btnRefreshW, btnRefreshH, "Refresh (rescan)", 0);
        }

        if (ev.type == ButtonPress && ev.xbutton.button == 1) {
            if (ev.xbutton.window == a.btnResize) {
                resizePressed = 1;
                drawButton(&a, a.btnResize, a.btnResizeW, a.btnResizeH, "> Resizing...", 1);
                XFlush(a.dpy);
                resizeMC(&a, 800, 600);
                drawStatus(&a);
            } else if (ev.xbutton.window == a.btnRestore) {
                restorePressed = 1;
                char lbl[64];
                snprintf(lbl, sizeof(lbl), "> Restoring...");
                drawButton(&a, a.btnRestore, a.btnRestoreW, a.btnRestoreH, lbl, 1);
                XFlush(a.dpy);
                resizeMC(&a, a.mcOrigW, a.mcOrigH);
                drawStatus(&a);
            } else if (ev.xbutton.window == btnRefresh) {
                refreshPressed = 1;
                drawButton(&a, btnRefresh, btnRefreshW, btnRefreshH, "> Scanning...", 1);
                XFlush(a.dpy);
                char *newName = NULL;
                Window newWin = findMinecraftWindow(a.dpy, root, &newName);
                if (newWin) {
                    a.mcWin = newWin;
                    XWindowAttributes at;
                    if (XGetWindowAttributes(a.dpy, a.mcWin, &at)) {
                        a.mcOrigW = at.width;
                        a.mcOrigH = at.height;
                    }
                    snprintf(a.statusText, sizeof(a.statusText),
                             "Minecraft 0x%lx \"%s\" — %dx%d (refreshed)",
                             a.mcWin, newName ? newName : "?", a.mcOrigW, a.mcOrigH);
                    free(newName);
                } else {
                    snprintf(a.statusText, sizeof(a.statusText), "Minecraft not found!");
                }
                drawStatus(&a);
            }
        }

        if (ev.type == ButtonRelease && ev.xbutton.button == 1) {
            if (ev.xbutton.window == a.btnResize) {
                resizePressed = 0;
                drawButton(&a, a.btnResize, a.btnResizeW, a.btnResizeH, "Resize to 800x600", 0);
            } else if (ev.xbutton.window == a.btnRestore) {
                restorePressed = 0;
                char lbl[64];
                snprintf(lbl, sizeof(lbl), "Restore %dx%d", a.mcOrigW, a.mcOrigH);
                drawButton(&a, a.btnRestore, a.btnRestoreW, a.btnRestoreH, lbl, 0);
            } else if (ev.xbutton.window == btnRefresh) {
                refreshPressed = 0;
                drawButton(&a, btnRefresh, btnRefreshW, btnRefreshH, "Refresh (rescan)", 0);
            }
        }
    }

    XCloseDisplay(a.dpy);
    return 0;
}
