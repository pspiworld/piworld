
#ifdef MESA

#include "pg_x11.h"

extern int terminate;

static X11_STATE_T _x11_state, *x11_state=&_x11_state;


EGLNativeDisplayType get_egl_display_id()
{
    x11_state->native_dpy = XOpenDisplay(x11_state->display_name);
    if (!x11_state->native_dpy)
        _pg_fatal("failed to initialize native display");

    return x11_state->native_dpy;
}


EGLNativeWindowType
get_egl_window_id(EGLConfig config, EGLDisplay display, uint32_t *w,
                  uint32_t *h, char *title)
{
    int x = 50;
    int y = 50;

    XVisualInfo *visInfo, visTemplate;
    int num_visuals;
    Window root, xwin;
    XSetWindowAttributes attr;
    unsigned long mask;
    EGLint vid;

    if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &vid))
        _pg_fatal("failed to get visual id");

    /* The X window visual must match the EGL config */
    visTemplate.visualid = vid;
    visInfo = XGetVisualInfo(x11_state->native_dpy,
                             VisualIDMask, &visTemplate, &num_visuals);
    if (!visInfo)
        _pg_fatal("failed to get an visual of id 0x%x", vid);

    root = RootWindow(x11_state->native_dpy,
                      DefaultScreen(x11_state->native_dpy));

    /* window attributes */
    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(x11_state->native_dpy,
                                    root, visInfo->visual, AllocNone);
    attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask |
                      KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                      PointerMotionMask;
    mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

    xwin = XCreateWindow(x11_state->native_dpy, root, x, y, *w, *h,
                         0, visInfo->depth, InputOutput, visInfo->visual, mask,
                         &attr);
    if (!xwin)
        _pg_fatal("failed to create a window");

    XFree(visInfo);

    /* set hints and properties */
    {
        XSizeHints sizehints;
        sizehints.x = x;
        sizehints.y = y;
        sizehints.width  = *w;
        sizehints.height = *h;
        sizehints.flags = USSize | USPosition;
        XSetNormalHints(x11_state->native_dpy, xwin, &sizehints);
        XSetStandardProperties(x11_state->native_dpy, xwin, title, title,
                               None, (char **) NULL, 0, &sizehints);
    }

    XMapWindow(x11_state->native_dpy, xwin);

    x11_event_init(x11_state->native_dpy, xwin);

    return xwin;
}

void pg_end()
{
    XCloseDisplay(x11_state->native_dpy);
}

EGLNativeDisplayType get_x11_display()
{
    return x11_state->native_dpy;
}

int pg_get_gpu_mem_size()
{
    // assume gpu has at least 512MiB available to it
    return 512;
}

#endif

