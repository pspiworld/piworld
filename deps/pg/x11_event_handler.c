// Some parts derived from GLFW - so those parts are licensed as:
//========================================================================
// GLFW 3.1 X11 - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2010 Camilla Berglund <elmindreda@elmindreda.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xcursor/Xcursor.h>
#include "x11_event_handler.h"

static X11_EVENT_STATE_T _x11_event_state, *x11_event_state=&_x11_event_state;
KeyPressHandler _key_press_handler;
KeyReleaseHandler _key_release_handler;
MotionHandler _motion_handler;
MousePressHandler _mouse_press_handler;
MouseReleaseHandler _mouse_release_handler;
WindowCloseHandler _window_close_handler;
FocusOutHandler _focus_out_handler;

static Cursor createInvisibleCursor(void);
void move_mouse_to_window_centre();


void x11_event_init(Display *display, Window window)
{
    x11_event_state->display = display;
    x11_event_state->window = window;
    x11_event_state->invisible_cursor = createInvisibleCursor();
    x11_event_state->ignore_next_motion_event = False;

    // Declare the WM protocols supported by pg
    x11_event_state->WM_PROTOCOLS = XInternAtom(x11_event_state->display,
                                    "WM_PROTOCOLS",
                                    False);

    x11_event_state->WM_DELETE_WINDOW = XInternAtom(x11_event_state->display,
                                        "WM_DELETE_WINDOW",
                                        False);
    int count = 0;
    Atom protocols[2];

    // The WM_DELETE_WINDOW ICCCM protocol
    // Basic window close notification protocol
    if (x11_event_state->WM_DELETE_WINDOW)
        protocols[count++] = x11_event_state->WM_DELETE_WINDOW;
    if (count > 0)
    {
        XSetWMProtocols(x11_event_state->display, window,
                        protocols, count);
    }

    set_mouse_relative();
}

void set_mouse_relative()
{
    Window root;
    root = XDefaultRootWindow(x11_event_state->display);
    int r;
    r = XGrabPointer(x11_event_state->display, root, False,
                     ButtonMotionMask | ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask, GrabModeAsync, GrabModeAsync,
                     root, x11_event_state->invisible_cursor, CurrentTime);
    move_mouse_to_window_centre();
    x11_event_state->mouse_is_relative = True;
}

void set_mouse_absolute()
{
    XUngrabPointer(x11_event_state->display, CurrentTime);
    x11_event_state->mouse_is_relative = False;
}

int relative_mouse_in_use() {
    return x11_event_state->mouse_is_relative && x11_event_state->has_focus;
}

void pg_next_event()
{
    XEvent event;
    int keySym;

    /* block for next event */
    while (XPending(x11_event_state->display)) {
        XNextEvent(x11_event_state->display, &event);
        Window root;
        root = XDefaultRootWindow(x11_event_state->display);

        switch (event.type) {
        case ConfigureNotify:
        {
            if (relative_mouse_in_use()) {
                // Reset mouse cursor as window manager may of changed it when
                // moving the window.
                set_mouse_relative();
            }
            break;
        }
        case ClientMessage:
        {
            if (event.xclient.message_type == x11_event_state->WM_PROTOCOLS) {
                const Atom protocol = event.xclient.data.l[0];

                if (protocol == x11_event_state->WM_DELETE_WINDOW) {
                    if (_window_close_handler) {
                        _window_close_handler();
                    }
                }
            }
            break;
        }
        case FocusIn:
        {
            x11_event_state->has_focus = 1;
            if (x11_event_state->mouse_is_relative) {
                set_mouse_relative();
            }
            break;
        }
        case FocusOut:
        {
            x11_event_state->has_focus = 0;
            if (x11_event_state->mouse_is_relative) {
                // release the mouse pointer when switching to another window
                XUngrabPointer(x11_event_state->display, CurrentTime);
            }
            if (_focus_out_handler) {
                _focus_out_handler();
            }
            break;
        }
        case KeyPress:
        {
            char buffer[1];
            KeySym sym;
            int r;

            r = XLookupString(&event.xkey,
                              buffer, sizeof(buffer), &sym, NULL);
            if (_key_press_handler) {
                _key_press_handler(buffer[0], event.xkey.state, sym);
            }
            break;
        }
        case KeyRelease:
        {
            char buffer[1];
            KeySym sym;
            int r;

            r = XLookupString(&event.xkey,
                              buffer, sizeof(buffer), &sym, NULL);
            if (_key_release_handler) {
                _key_release_handler(buffer[0], sym);
            }
            break;
        }
        case ButtonPress:
        {
            if (_mouse_press_handler) {
                _mouse_press_handler(event.xbutton.button, event.xkey.state);
            }
            break;
        }
        case ButtonRelease:
        {
            if (_mouse_release_handler) {
                _mouse_release_handler(event.xbutton.button, event.xkey.state);
            }
            break;
        }
        case MotionNotify:
        {
            if (relative_mouse_in_use()) {
                XWindowAttributes window_attrs;
                XGetWindowAttributes(x11_event_state->display,
                                     x11_event_state->window, &window_attrs);
                int cx = window_attrs.width / 2;
                int cy = window_attrs.height / 2;
                if ((event.xmotion.x == cx && event.xmotion.y == cy)
                        || x11_event_state->ignore_next_motion_event) {
                    // No mouse motion to report (mouse is at window centre)
                    if (x11_event_state->ignore_next_motion_event) {
                        x11_event_state->ignore_next_motion_event--;
                        if (x11_event_state->ignore_next_motion_event < 0) {
                            x11_event_state->ignore_next_motion_event = 0;
                        }
                    }
                }
                else {
                    int diff_x = cx - event.xmotion.x;
                    int diff_y = cy - event.xmotion.y;
                    if (_motion_handler) {
                        _motion_handler(diff_x, diff_y);
                    }
                    x11_event_state->accumulative_mouse_motion_x +=
                        diff_x;  // adds to the motion - until handled
                    x11_event_state->accumulative_mouse_motion_y += diff_y;
                    // Move mouse back to window centre
                    XWarpPointer(x11_event_state->display, None,
                                 event.xmotion.window, 0, 0, 0, 0, cx, cy);
                }
            }
            break;
        }
        default:
            ; /*no-op*/
        }
    }
}

void set_key_press_handler(KeyPressHandler key_press_handler)
{
    _key_press_handler = key_press_handler;
}

void set_key_release_handler(KeyReleaseHandler key_release_handler)
{
    _key_release_handler = key_release_handler;
}

void set_motion_handler(MotionHandler motion_handler)
{
    _motion_handler = motion_handler;
}

void set_mouse_press_handler(MousePressHandler mouse_press_handler)
{
    _mouse_press_handler = mouse_press_handler;
}

void set_mouse_release_handler(MouseReleaseHandler mouse_release_handler)
{
    _mouse_release_handler = mouse_release_handler;
}

void set_window_close_handler(WindowCloseHandler window_close_handler)
{
    _window_close_handler = window_close_handler;
}

void set_focus_out_handler(FocusOutHandler focus_out_handler)
{
    _focus_out_handler = focus_out_handler;
}

void get_x11_accumulative_mouse_motion(int *x, int *y)
{
    *x = x11_event_state->accumulative_mouse_motion_x;
    *y = x11_event_state->accumulative_mouse_motion_y;
}

// Create a blank cursor for hidden and disabled cursor modes
static Cursor createInvisibleCursor(void)
{
    unsigned char pixels[16 * 16 * 4];

    memset(pixels, 0, sizeof(pixels));

    int i;
    Cursor cursor;

    XcursorImage* native = XcursorImageCreate(16, 16);
    if (native == NULL)
        return None;

    native->xhot = 0;
    native->yhot = 0;

    unsigned char* source = (unsigned char*) pixels;
    XcursorPixel* target = native->pixels;

    for (i = 0;  i < 16 * 16;  i++, target++, source += 4) {
        *target = (source[3] << 24) |
                  (source[0] << 16) |
                  (source[1] <<  8) |
                  source[2];
    }

    cursor = XcursorImageLoadCursor(x11_event_state->display, native);
    XcursorImageDestroy(native);

    return cursor;
}

void move_mouse_to_window_centre()
{
    Window root;
    root = XDefaultRootWindow(x11_event_state->display);
    XWindowAttributes window_attrs;
    XGetWindowAttributes(x11_event_state->display, x11_event_state->window,
                         &window_attrs);
    int x = window_attrs.width / 2;
    int y = window_attrs.height / 2;
    x11_event_state->ignore_next_motion_event++;
    XWarpPointer(x11_event_state->display, None, root, 0, 0, 0, 0, x, y);
    XSync(x11_event_state->display, False);
}

void _pg_fatal(char *format, ...)
{
    va_list args;

    va_start(args, format);

    fprintf(stderr, "pg: ");
    vfprintf(stderr, format, args);
    va_end(args);
    putc('\n', stderr);

    exit(1);
}

