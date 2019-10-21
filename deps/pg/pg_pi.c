
#ifndef MESA

#include "pg.h"
#include "pg_pi.h"

#define RASPBIAN_X11_WINDOW_OFFSET_X -2
#define RASPBIAN_X11_WINDOW_OFFSET_Y -30
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static PI_STATE_T _pi_state, *pi_state=&_pi_state;
static EGL_DISPMANX_WINDOW_T nativewindow;

void _open_x11_window(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                      char *title);


EGLNativeDisplayType get_egl_display_id(void)
{
    bcm_host_init();

    pi_state->x11_display = XOpenDisplay(pi_state->x11_display_name);
    if (!pi_state->x11_display)
        _pg_fatal("failed to initialize native display");

    return EGL_DEFAULT_DISPLAY;
}

int get_reduction_factor(int w, int h) {
    int reduction = 1;
    if (pg_get_gpu_mem_size() < 128) {
        if ((w * h) > (1680 * 1024)) {
            reduction = 3;
        } else if ((w * h) > (640 * 480)) {
            reduction = 2;
        }
    }
    return reduction;
}

EGLNativeWindowType get_egl_window_id(__attribute__((unused)) EGLConfig config,
                                      __attribute__((unused)) EGLDisplay display,
                                      int *x, int *y,
                                      uint32_t *w, uint32_t *h,
                                      char *title)
{
    int32_t success = 0;
    int x11_x, x11_y;

    x11_x = *x - RASPBIAN_X11_WINDOW_OFFSET_X;
    x11_y = *y - RASPBIAN_X11_WINDOW_OFFSET_Y;

    DISPMANX_UPDATE_HANDLE_T dispman_update;
    VC_RECT_T src_rect;

    success = graphics_get_display_size(0 /* LCD */, &pi_state->screen_w,
                                        &pi_state->screen_h);
    assert( success >= 0 );
    *w = MIN(pi_state->screen_w, *w);
    *h = MIN(pi_state->screen_h, *h);

    // Centre output area inside screen area
    Window root;
    root = RootWindow(pi_state->x11_display,
                      DefaultScreen(pi_state->x11_display));
    XWindowAttributes window_attrs;
    XGetWindowAttributes(pi_state->x11_display, root, &window_attrs);
    if (*x == CENTRE_IN_SCREEN) {
        *x = (pi_state->screen_w/2) - (*w/2);
        x11_x = (window_attrs.width/2) - (*w/2) + RASPBIAN_X11_WINDOW_OFFSET_X;
    }
    if (*y == CENTRE_IN_SCREEN) {
        *y = (pi_state->screen_h/2) - (*h/2);
        x11_y = (window_attrs.height/2) - (*h/2) + RASPBIAN_X11_WINDOW_OFFSET_Y;
    }

    pi_state->dst_rect.x = *x;
    pi_state->dst_rect.y = *y;
    pi_state->dst_rect.width = *w;
    pi_state->dst_rect.height = *h;

    int reduction = get_reduction_factor(*w, *h);

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = (*w/reduction) << 16;
    src_rect.height = (*h/reduction) << 16;

    pi_state->dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
    dispman_update = vc_dispmanx_update_start( 0 );

    pi_state->dispman_element = vc_dispmanx_element_add ( dispman_update,
                                pi_state->dispman_display,
                                0/*layer*/, &pi_state->dst_rect, 0/*src*/,
                                &src_rect, DISPMANX_PROTECTION_NONE,
                                0 /*alpha*/, 0/*clamp*/, 0/*transform*/);

    nativewindow.element = pi_state->dispman_element;
    nativewindow.width = *w;
    nativewindow.height = *h;
    vc_dispmanx_update_submit_sync( dispman_update );

    _open_x11_window(x11_x, x11_y, *w, *h, title);

    return (EGLNativeWindowType)&nativewindow;
}


void pg_end(void)
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

    x11_event_init(pi_state->x11_display, xwin, x, y, w, h);
}

EGLNativeDisplayType get_x11_display(void)
{
    return pi_state->x11_display;
}

int pg_get_gpu_mem_size(void)
{
    char response[80] = "";
    int gpu_mem = 0;
    if (vc_gencmd(response, sizeof response, "get_mem gpu") == 0)
        vc_gencmd_number_property(response, "gpu", &gpu_mem);
    return gpu_mem;
}

void pi_set_window_geometry(int *x, int *y, uint32_t *w, uint32_t *h)
{
    DISPMANX_UPDATE_HANDLE_T dispman_update;
    VC_RECT_T src_rect;
    Screen *screen;

    // Account for screen offset
    screen = DefaultScreenOfDisplay(pi_state->x11_display);
    *x += (pi_state->screen_w - WidthOfScreen(screen)) / 2;
    *y += (pi_state->screen_h - HeightOfScreen(screen)) / 2;

    int reduction = get_reduction_factor(*w, *h);

    vc_dispmanx_rect_set(&src_rect, 0, 0, (*w/reduction) << 16,
                         (*h/reduction) << 16);
    vc_dispmanx_rect_set(&pi_state->dst_rect, *x, *y, *w, *h);

    *w = (*w/reduction);
    *h = (*h/reduction);

    dispman_update = vc_dispmanx_update_start(0);
    vc_dispmanx_element_change_attributes(dispman_update,
        pi_state->dispman_element, 0, 0, 0, &pi_state->dst_rect, &src_rect, 0,
        DISPMANX_NO_ROTATE);
    vc_dispmanx_update_submit_sync(dispman_update);

    nativewindow.width = *w;
    nativewindow.height = *h;
}

#endif

