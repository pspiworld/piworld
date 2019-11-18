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
#include <X11/extensions/XInput2.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/XKBlib.h>
#include "pg.h"
#include "x11_event_handler.h"

#define _NET_WM_STATE_REMOVE  0    /* remove/unset property */
#define _NET_WM_STATE_ADD     1    /* add/set property */
#define _NET_WM_STATE_TOGGLE  2    /* toggle property  */

static int xi2_opcode = 0;
static int events_selected = 0;

static X11_EVENT_STATE_T _x11_event_state, *x11_event_state=&_x11_event_state;
KeyPressHandler _key_press_handler;
KeyReleaseHandler _key_release_handler;
MouseMotionHandler _mouse_motion_handler;
MousePressHandler _mouse_press_handler;
MouseReleaseHandler _mouse_release_handler;
WindowCloseHandler _window_close_handler;
FocusOutHandler _focus_out_handler;

void select_events(void);
void deselect_events(void);
static Cursor createInvisibleCursor(void);
void handle_xinput2_event(const XIRawEvent *e);

void x11_event_init(Display *display, Window window, int x, int y, int w, int h)
{
    x11_event_state->display = display;
    x11_event_state->window = window;
    x11_event_state->invisible_cursor = createInvisibleCursor();
    x11_event_state->x = x;
    x11_event_state->y = y;
    x11_event_state->width = w;
    x11_event_state->height = h;

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

    /* XInput Extension available? */
    int event, error;
    if (!XQueryExtension(display, "XInputExtension", &xi2_opcode, &event,
                         &error)) {
        printf("X Input extension not available.\n");
        return;
    }

    /* Which version of XI2? We support 2.2 */
    int major = 2, minor = 2;
    if (XIQueryVersion(display, &major, &minor) == BadRequest) {
        printf("XI2 not available. Server supports %d.%d\n", major, minor);
        return;
    }

    select_events();
}

void select_events(void)
{
    if (events_selected) {
        // do not double select events
        return;
    }
    events_selected = 1;

    XIEventMask eventmask;
    unsigned char mask[3] = { 0, 0, 0 } ; /* the actual mask */

    eventmask.deviceid = XIAllDevices;
    eventmask.mask_len = sizeof(mask); /* always in bytes */
    eventmask.mask = mask;
    /* now set the mask */
    XISetMask(mask, XI_RawButtonPress);
    XISetMask(mask, XI_RawButtonRelease);
    XISetMask(mask, XI_RawKeyPress);
    XISetMask(mask, XI_RawKeyRelease);
    XISetMask(mask, XI_RawMotion);

    XISelectEvents(x11_event_state->display,
                   DefaultRootWindow(x11_event_state->display), &eventmask, 1);
}

void deselect_events(void)
{
    if (!events_selected) {
        // do not double deselect events
        return;
    }
    events_selected = 0;

    XIEventMask eventmask;
    unsigned char mask[1] = { 0 } ; /* the actual mask */

    eventmask.deviceid = XIAllDevices;
    eventmask.mask_len = sizeof(mask); /* always in bytes */
    eventmask.mask = mask;

    XISelectEvents(x11_event_state->display,
                   DefaultRootWindow(x11_event_state->display), &eventmask, 1);

    // Clear modifiers
    memset(x11_event_state->keyboard_mods, 0,
           sizeof(x11_event_state->keyboard_mods));
}

void set_mouse_relative()
{
    Window root;
    root = XDefaultRootWindow(x11_event_state->display);
    int rc;
    XIEventMask eventmask;
    unsigned char mask[3] = { 0, 0, 0 } ; /* the actual mask */

    eventmask.deviceid = XIAllDevices;
    eventmask.mask_len = sizeof(mask); /* always in bytes */
    eventmask.mask = mask;
    XISetMask(eventmask.mask, XI_RawMotion);
    XISetMask(eventmask.mask, XI_RawButtonPress);
    XISetMask(eventmask.mask, XI_RawButtonRelease);
    if ((rc = XIGrabDevice(x11_event_state->display, 2, root, CurrentTime,
                          x11_event_state->invisible_cursor, GrabModeAsync,
                          GrabModeAsync, False, &eventmask)) != GrabSuccess) {
        fprintf(stderr, "Grab failed with %d\n", rc);
        return;
    }

    x11_event_state->mouse_is_relative = True;
}

void set_mouse_absolute()
{
    XIUngrabDevice(x11_event_state->display, 2, CurrentTime);
    x11_event_state->mouse_is_relative = False;
}

int relative_mouse_in_use(void) {
    return x11_event_state->mouse_is_relative && x11_event_state->has_focus;
}

void pg_next_event()
{
    XEvent event;

    /* block for next event */
    while (XPending(x11_event_state->display)) {
        XNextEvent(x11_event_state->display, &event);

        switch (event.type) {
        case ConfigureNotify:
        {
            if ((event.xconfigure.x != x11_event_state->x ||
                event.xconfigure.y != x11_event_state->y) &&
                !event.xconfigure.above) {
                x11_event_state->x = event.xconfigure.x;
                x11_event_state->y = event.xconfigure.y;
            }
            if (event.xconfigure.width != x11_event_state->width ||
                event.xconfigure.height != x11_event_state->height) {
                x11_event_state->width = event.xconfigure.width;
                x11_event_state->height = event.xconfigure.height;
            }
            pg_set_window_geometry(x11_event_state->x, x11_event_state->y,
                x11_event_state->width, x11_event_state->height);
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
            select_events();
            break;
        }
        case FocusOut:
        {
            deselect_events();
            x11_event_state->has_focus = 0;
            if (x11_event_state->mouse_is_relative) {
                // release the mouse pointer when switching to another window
                XIUngrabDevice(x11_event_state->display, 2, CurrentTime);
            }
            if (_focus_out_handler) {
                _focus_out_handler();
            }
            break;
        }
        case GenericEvent:
        {
            if (event.xcookie.extension == xi2_opcode &&
                XGetEventData(x11_event_state->display, &event.xcookie)) {
                const XIRawEvent *e = (const XIRawEvent *) event.xcookie.data;
                if (e->deviceid > 3) {
                    handle_xinput2_event(e);
                }
                XFreeEventData(x11_event_state->display, &event.xcookie);
            }
            break;
        }
        default:
            ; /*no-op*/
        }
    }
}

void handle_xinput2_event(const XIRawEvent *e)
{
    switch(e->evtype)
    {
    case XI_RawButtonPress:
        if (_mouse_press_handler) {
            _mouse_press_handler(e->deviceid, e->detail);
        }
        break;
    case XI_RawButtonRelease:
        if (_mouse_release_handler) {
            _mouse_release_handler(e->deviceid, e->detail);
        }
        break;
    case XI_RawMotion:
        if (relative_mouse_in_use()) {
            if (_mouse_motion_handler) {
                int x = 0;
                int y = 0;
                double *value = e->raw_values;
                if (XIMaskIsSet(e->valuators.mask, 0)) {
                    x = (int) *value;
                    value++;
                }
                if (XIMaskIsSet(e->valuators.mask, 1)) {
                    y = (int) *value;
                }
                _mouse_motion_handler(e->sourceid, -x, -y);
            }
        }
        break;
    case XI_RawKeyPress:
        if (_key_press_handler) {
            KeySym sym;
            XkbLookupKeySym(x11_event_state->display, e->detail,
                x11_event_state->keyboard_mods[e->deviceid], NULL, &sym);

            if (sym >= XK_Shift_L && sym <= XK_Hyper_R) {
                int key_mask = 0;
                if (sym == XK_Shift_L || sym == XK_Shift_R) {
                    key_mask = ShiftMask;
                } else if (sym == XK_Control_L || sym == XK_Control_R) {
                    key_mask = ControlMask;
                }
                x11_event_state->keyboard_mods[e->deviceid] |= key_mask;
            }

            _key_press_handler(e->deviceid,
                x11_event_state->keyboard_mods[e->deviceid], sym);
        }
        break;
    case XI_RawKeyRelease:
        if (_key_release_handler) {
            KeySym sym;
            XkbLookupKeySym(x11_event_state->display, e->detail,
                x11_event_state->keyboard_mods[e->deviceid], NULL, &sym);

            if (sym >= XK_Shift_L && sym <= XK_Hyper_R) {
                int key_mask = 0;
                if (sym == XK_Shift_L || sym == XK_Shift_R) {
                    key_mask = ShiftMask;
                } else if (sym == XK_Control_L || sym == XK_Control_R) {
                    key_mask = ControlMask;
                }
                x11_event_state->keyboard_mods[e->deviceid] &= ~key_mask;
            }

            _key_release_handler(e->deviceid, sym);
        }
        break;
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

void set_mouse_motion_handler(MouseMotionHandler mouse_motion_handler)
{
    _mouse_motion_handler = mouse_motion_handler;
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

void pg_move_window(int x, int y)
{
    XMoveWindow(x11_event_state->display, x11_event_state->window, x, y);
}

void pg_resize_window(int width, int height)
{
    XResizeWindow(x11_event_state->display, x11_event_state->window, width, height);
}

void pg_set_window_title(char *title)
{
    XStoreName(x11_event_state->display, x11_event_state->window, title);
}

void pg_toggle_fullscreen()
{
    pg_fullscreen(!x11_event_state->fullscreen);
}

void pg_fullscreen(int fullscreen)
{
    XEvent event;
    Atom   state_atom;
    Atom   fs_atom;
    Window root;

    x11_event_state->fullscreen = fullscreen;
    state_atom = XInternAtom(x11_event_state->display,
                             "_NET_WM_STATE", False);
    fs_atom    = XInternAtom(x11_event_state->display,
                             "_NET_WM_STATE_FULLSCREEN", False);

    event.xclient.type          = ClientMessage;
    event.xclient.serial        = 0;
    event.xclient.send_event    = True;
    event.xclient.window        = x11_event_state->window;
    event.xclient.message_type  = state_atom;
    event.xclient.format        = 32;
    event.xclient.data.l[0]     = fullscreen ? _NET_WM_STATE_ADD :
                                               _NET_WM_STATE_REMOVE;
    event.xclient.data.l[1]     = fs_atom;
    event.xclient.data.l[2]     = 0;

    root = XDefaultRootWindow(x11_event_state->display);
    XSendEvent(x11_event_state->display, root, False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
}

int pg_get_mods(int keyboard_id)
{
    if (keyboard_id < 0 || keyboard_id >= MAX_DEVICE_COUNT) {
        return 0;
    }
    return x11_event_state->keyboard_mods[keyboard_id];
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

