
#pragma once

#include <stdio.h>

#include "GLES2/gl2.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#define CENTRE_IN_SCREEN -99999
#define MAX_DEVICE_COUNT 32

typedef struct
{
    Window window;  // backing window for pi, render window for mesa
    Display *display;
    Cursor invisible_cursor;
    int mouse_is_relative;
    int keyboard_mods[MAX_DEVICE_COUNT];
    int has_focus;
    Atom WM_PROTOCOLS;
    Atom WM_DELETE_WINDOW;
    int width, height;
    int x, y;
    int fullscreen;
    int fullscreen_width;
    int fullscreen_height;
} X11_EVENT_STATE_T;

void x11_event_init(Display *display, Window window, int x, int y, int w, int h);
void set_mouse_relative(void);
void set_mouse_absolute(void);
int relative_mouse_in_use(void);

void pg_next_event(void);

typedef void (*KeyPressHandler)(int keyboard_id, int mods, int keysym);
typedef void (*KeyReleaseHandler)(int keyboard_id, int keysym);
typedef void (*MousePressHandler)(int mouse_id, int button);
typedef void (*MouseReleaseHandler)(int mouse_id, int button);
typedef void (*MouseMotionHandler)(int mouse_id, float x, float y);
typedef void (*WindowCloseHandler)(void);
typedef void (*FocusOutHandler)(void);

void set_key_press_handler(KeyPressHandler key_press_handler);
void set_key_release_handler(KeyReleaseHandler key_release_handler);
void set_mouse_press_handler(MousePressHandler mouse_press_handler);
void set_mouse_release_handler(MouseReleaseHandler mouse_release_handler);
void set_mouse_motion_handler(MouseMotionHandler mouse_motion_handler);
void set_window_close_handler(WindowCloseHandler window_close_handler);
void set_focus_out_handler(FocusOutHandler focus_out_handler);

void pg_move_window(int x, int y);
void pg_resize_window(int width, int height);
void pg_set_window_title(char *title);
void pg_fullscreen(int fullscreen);
void pg_toggle_fullscreen(void);
void pg_set_fullscreen_size(int fullscreen_width, int fullscreen_height);
void pg_restore_original_fullscreen_size(void);
int pg_get_mods(int keyboard_id);

void _pg_fatal(char *format, ...);

