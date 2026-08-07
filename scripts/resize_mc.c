// resize_mc: XResizeWindow на окне Minecraft
#include <X11/Xlib.h>
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: %s <window_id>\n", argv[0]); return 1; }
    Window win = (Window)strtoul(argv[1], NULL, 16);
    Display *dpy = XOpenDisplay(":1");
    if (!dpy) { perror("XOpenDisplay"); return 1; }

    // Получить текущий размер
    XWindowAttributes attrs;
    XGetWindowAttributes(dpy, win, &attrs);
    printf("Current: %dx%d\n", attrs.width, attrs.height);

    // Пробуем XResizeWindow
    printf("=== XResizeWindow(%d,%d) ===\n", 800, 500);
    XResizeWindow(dpy, win, 800, 500);
    XFlush(dpy);

    // Ждём и проверяем
    sleep(1);
    XGetWindowAttributes(dpy, win, &attrs);
    printf("After XResizeWindow: %dx%d\n", attrs.width, attrs.height);

    // XConfigureWindow
    printf("=== XConfigureWindow(700,400) ===\n");
    XWindowChanges changes;
    changes.width = 700;
    changes.height = 400;
    XConfigureWindow(dpy, win, CWWidth | CWHeight, &changes);
    XFlush(dpy);
    sleep(1);
    XGetWindowAttributes(dpy, win, &attrs);
    printf("After XConfigureWindow: %dx%d\n", attrs.width, attrs.height);

    XCloseDisplay(dpy);
    return 0;
}
