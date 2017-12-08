
#pragma once

#include <stdio.h>
#include <string.h>
#include "GLES2/gl2.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "x11_event_handler.h"

typedef struct
{
    EGLNativeWindowType nativewindow;
    EGLNativeDisplayType native_dpy;
    const char *display_name;
} X11_STATE_T;

EGLNativeDisplayType get_egl_display_id();
EGLNativeWindowType get_egl_window_id(EGLConfig config, EGLDisplay display,
                                      uint32_t *w, uint32_t *h, char *title);
void platform_exit();
EGLNativeDisplayType get_x11_display();

