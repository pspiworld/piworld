
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

    // X11 stuff
    EGLNativeDisplayType x11_display;
    const char *x11_display_name;
} PI_STATE_T;

EGLNativeDisplayType get_egl_display_id();
EGLNativeWindowType get_egl_window_id(EGLConfig config, EGLDisplay display,
                                      uint32_t *w, uint32_t *h, char *title);
void platform_exit();
EGLNativeDisplayType get_x11_display();

