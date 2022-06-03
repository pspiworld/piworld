
#pragma once

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "GLES/gl.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "bcm_host.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "x11_event_handler.h"

typedef struct
{
    DISPMANX_DISPLAY_HANDLE_T dispman_display;
    DISPMANX_ELEMENT_HANDLE_T dispman_element;
    EGLNativeWindowType nativewindow;
    VC_RECT_T dst_rect;
    uint32_t screen_w, screen_h;

    // X11 stuff
    EGLNativeDisplayType x11_display;
    const char *x11_display_name;
} PI_STATE_T;

EGLNativeDisplayType get_egl_display_id(void);
EGLNativeWindowType get_egl_window_id(EGLConfig config, EGLDisplay display,
                                      int *x, int *y,
                                      uint32_t *w, uint32_t *h, char *title);
void platform_exit(void);
EGLNativeDisplayType get_x11_display(void);
void pi_set_window_geometry(int *x, int *y, uint32_t *w, uint32_t *h, int fullscreen);

