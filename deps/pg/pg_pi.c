
#ifndef MESA

#include "pg_pi.h"

#define RASPBIAN_X11_WINDOW_OFFSET_X -2
#define RASPBIAN_X11_WINDOW_OFFSET_Y -30

static PI_STATE_T _pi_state, *pi_state=&_pi_state;

void _open_x11_window(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                      char *title);


EGLNativeDisplayType get_egl_display_id()
{
    bcm_host_init();

    pi_state->x11_display = XOpenDisplay(pi_state->x11_display_name);
    if (!pi_state->x11_display)
        _pg_fatal("failed to initialize native display");

    return EGL_DEFAULT_DISPLAY;
}


EGLNativeWindowType get_egl_window_id(EGLConfig config, EGLDisplay display,
                                      uint32_t *w, uint32_t *h, char *title)
{
    int32_t success = 0;
    static EGL_DISPMANX_WINDOW_T nativewindow;
    int screen_w, screen_h;
    int cx, cy;

    DISPMANX_UPDATE_HANDLE_T dispman_update;
    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;

    success = graphics_get_display_size(0 /* LCD */, &screen_w, &screen_h);
    assert( success >= 0 );

    // Centre output area inside screen area
    Window root;
    int x_offset;
    int y_offset;
    int x11_cx;
    int x11_cy;
    root = RootWindow(pi_state->x11_display,
                      DefaultScreen(pi_state->x11_display));
    XWindowAttributes window_attrs;
    XGetWindowAttributes(pi_state->x11_display, root, &window_attrs);
    x11_cx = (window_attrs.width/2) - (*w/2) + RASPBIAN_X11_WINDOW_OFFSET_X;
    x11_cy = (window_attrs.height/2) - (*h/2) + RASPBIAN_X11_WINDOW_OFFSET_Y;
    cx = (screen_w/2) - (*w/2);
    cy = (screen_h/2) - (*h/2);

    dst_rect.x = cx;
    dst_rect.y = cy;
    dst_rect.width = *w;
    dst_rect.height = *h;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = *w << 16;
    src_rect.height = *h << 16;

    pi_state->dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
    dispman_update = vc_dispmanx_update_start( 0 );

    pi_state->dispman_element = vc_dispmanx_element_add ( dispman_update,
                                pi_state->dispman_display,
                                0/*layer*/, &dst_rect, 0/*src*/,
                                &src_rect, DISPMANX_PROTECTION_NONE,
                                0 /*alpha*/, 0/*clamp*/, 0/*transform*/);

    nativewindow.element = pi_state->dispman_element;
    nativewindow.width = *w;
    nativewindow.height = *h;
    vc_dispmanx_update_submit_sync( dispman_update );

    _open_x11_window(x11_cx, x11_cy, *w, *h, title);

    return (EGLNativeWindowType)&nativewindow;
}


void pg_end()
{
    DISPMANX_UPDATE_HANDLE_T dispman_update;
    int s;

    dispman_update = vc_dispmanx_update_start( 0 );
    s = vc_dispmanx_element_remove(dispman_update, pi_state->dispman_element);
    assert(s == 0);
    vc_dispmanx_update_submit_sync( dispman_update );
    s = vc_dispmanx_display_close(pi_state->dispman_display);
    assert (s == 0);
}


/*
 * Use an X11 window to capture mouse and keyboard input.
 * An X11 window the same size and position as the vc4 opengl-es area will
 * be opened.
 */
void _open_x11_window(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                      char *title)
{
    Window root, xwin;
    XSetWindowAttributes attr;
    unsigned long mask;

    root = RootWindow(pi_state->x11_display, DefaultScreen(pi_state->x11_display));

    /* window attributes */
    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask |
                      KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                      PointerMotionMask | FocusChangeMask;
    mask = CWBackPixel | CWBorderPixel | CWEventMask;

    xwin = XCreateWindow(pi_state->x11_display, root, x, y, w, h, 0,
                         CopyFromParent, InputOutput, CopyFromParent, mask,
                         &attr);
    if (!xwin)
        _pg_fatal("failed to create a window");

    /* set hints and properties */
    {
        XSizeHints sizehints;
        sizehints.x = x;
        sizehints.y = y;
        sizehints.width  = w;
        sizehints.height = h;
        sizehints.flags = USSize | USPosition;
        XSetNormalHints(pi_state->x11_display, xwin, &sizehints);
        XSetStandardProperties(pi_state->x11_display, xwin, title,
                               title, None, (char **) NULL, 0, &sizehints);
    }

    XMapWindow(pi_state->x11_display, xwin);

    x11_event_init(pi_state->x11_display, xwin);
}

EGLNativeDisplayType get_x11_display()
{
    return pi_state->x11_display;
}

int pg_get_gpu_mem_size()
{
    char response[80] = "";
    int gpu_mem = 0;
    if (vc_gencmd(response, sizeof response, "get_mem gpu") == 0)
        vc_gencmd_number_property(response, "gpu", &gpu_mem);
    return gpu_mem;
}

#endif

