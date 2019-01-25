// Some parts derived from GLFW - so those parts are licensed as:
//========================================================================
// GLFW 3.1 Linux - www.glfw.org
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

#include "pg_joystick.h"

#include <linux/joystick.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct _PG_Joystick
{
    PG_Joystick  js[PG_JOYSTICK_LAST + 1];
    int          inotify;
    int          watch;
    regex_t      regex;
} _PG_Joystick;
static _PG_Joystick pg_js;

JoystickAxisHandler _joystick_axis_handler;
JoystickButtonHandler _joystick_button_handler;

#define PG_RELEASE 0
#define PG_PRESS   1

// Attempt to open the specified joystick device
//
static void open_joystick_device(const char* path)
{
    char axis_count, button_count;
    char name[256];
    int joy, fd, version;

    for (joy = PG_JOYSTICK_1; joy <= PG_JOYSTICK_LAST; joy++) {
        if (!pg_js.js[joy].present) {
            continue;
        }

        if (strcmp(pg_js.js[joy].path, path) == 0) {
            return;
        }
    }

    for (joy = PG_JOYSTICK_1; joy <= PG_JOYSTICK_LAST; joy++) {
        if (!pg_js.js[joy].present) {
            break;
        }
    }

    if (joy > PG_JOYSTICK_LAST) {
        return;
    }

    fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        return;
    }

    pg_js.js[joy].fd = fd;

    // Verify that the joystick driver version is at least 1.0
    ioctl(fd, JSIOCGVERSION, &version);
    if (version < 0x010000) {
        // It's an old 0.x interface (we don't support it)
        close(fd);
        return;
    }

    if (ioctl(fd, JSIOCGNAME(sizeof(name)), name) < 0) {
        strncpy(name, "Unknown", sizeof(name));
    }

    pg_js.js[joy].name = strdup(name);
    pg_js.js[joy].path = strdup(path);

    ioctl(fd, JSIOCGAXES, &axis_count);
    pg_js.js[joy].axis_count = (int) axis_count;

    ioctl(fd, JSIOCGBUTTONS, &button_count);
    pg_js.js[joy].button_count = (int) button_count;

    pg_js.js[joy].axes = calloc(axis_count, sizeof(float));
    pg_js.js[joy].buttons = calloc(button_count, 1);

    pg_js.js[joy].present = 1;
}

// Polls for and processes events for all present joysticks
//
void pg_poll_joystick_events(void)
{
    int i;
    struct js_event e;
    ssize_t offset = 0;
    char buffer[16384];

    const ssize_t size = read(pg_js.inotify, buffer, sizeof(buffer));

    while (size > offset) {
        regmatch_t match;
        const struct inotify_event* e =
            (struct inotify_event*) (buffer + offset);

        if (regexec(&pg_js.regex, e->name, 1, &match, 0) == 0) {
            char path[20];
            snprintf(path, sizeof(path), "/dev/input/%s", e->name);
            open_joystick_device(path);
        }

        offset += sizeof(struct inotify_event) + e->len;
    }

    for (i = 0; i <= PG_JOYSTICK_LAST; i++) {
        if (!pg_js.js[i].present) {
            continue;
        }

        // Read all queued events (non-blocking)
        for (;;) {
            errno = 0;
            if (read(pg_js.js[i].fd, &e, sizeof(e)) < 0) {
                if (errno == ENODEV) {
                    // The joystick was disconnected

                    free(pg_js.js[i].axes);
                    free(pg_js.js[i].buttons);
                    free(pg_js.js[i].name);
                    free(pg_js.js[i].path);

                    memset(&pg_js.js[i], 0, sizeof(pg_js.js[i]));
                }

                break;
            }

            // We don't care if it's an init event or not
            e.type &= ~JS_EVENT_INIT;

            switch (e.type) {
                case JS_EVENT_AXIS:
                    pg_js.js[i].axes[e.number] =
                        (float) e.value / 32767.0f;
                    if (_joystick_axis_handler) {
                        _joystick_axis_handler(&pg_js.js[i], i, e.number,
                                               pg_js.js[i].axes[e.number]);
                    }
                    break;

                case JS_EVENT_BUTTON:
                    pg_js.js[i].buttons[e.number] =
                        e.value ? PG_PRESS : PG_RELEASE;
                    if (_joystick_button_handler) {
                        _joystick_button_handler(&pg_js.js[i], i, e.number,
                                              pg_js.js[i].buttons[e.number]);
                    }
                    break;

                default:
                    break;
            }
        }
    }
}

// Initialize joystick interface
//
int pg_init_joysticks(void)
{
    const char* dirname = "/dev/input";
    DIR* dir;

    pg_js.inotify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (pg_js.inotify == -1) {
        printf("Linux: Failed to initialize inotify: %s\n",
               strerror(errno));
        return 0;
    }

    // HACK: Register for IN_ATTRIB as well to get notified when udev is done
    //       This works well in practice but the true way is libudev

    pg_js.watch = inotify_add_watch(pg_js.inotify,
                                       dirname,
                                       IN_CREATE | IN_ATTRIB);
    if (pg_js.watch == -1) {
        printf("Linux: Failed to watch for joystick connections in %s: %s\n",
               dirname,
               strerror(errno));
        // Continue without device connection notifications
    }

    if (regcomp(&pg_js.regex, "^js[0-9]\\+$", 0) != 0) {
        printf("Linux: Failed to compile regex\n");
        return 0;
    }

    dir = opendir(dirname);
    if (dir) {
        struct dirent* entry;

        while ((entry = readdir(dir))) {
            char path[20];
            regmatch_t match;

            if (regexec(&pg_js.regex, entry->d_name, 1, &match, 0) != 0) {
                continue;
            }

            snprintf(path, sizeof(path), "%s/%s", dirname, entry->d_name);
            open_joystick_device(path);
        }

        closedir(dir);
    }
    else {
        printf("Linux: Failed to open joystick device directory %s: %s\n",
               dirname,
               strerror(errno));
        // Continue with no joysticks detected
    }

    return 1;
}

// Close all opened joystick handles
//
void pg_terminate_joysticks(void)
{
    int i;

    for (i = 0; i <= PG_JOYSTICK_LAST; i++) {
        if (pg_js.js[i].present) {
            close(pg_js.js[i].fd);
            free(pg_js.js[i].axes);
            free(pg_js.js[i].buttons);
            free(pg_js.js[i].name);
            free(pg_js.js[i].path);
        }
    }

    regfree(&pg_js.regex);

    if (pg_js.inotify > 0) {
        if (pg_js.watch > 0) {
            inotify_rm_watch(pg_js.inotify, pg_js.watch);
        }

        close(pg_js.inotify);
    }
}

void pg_set_joystick_axis_handler(JoystickAxisHandler joystick_axis_handler)
{
    _joystick_axis_handler = joystick_axis_handler;
}

void pg_set_joystick_button_handler(JoystickButtonHandler joystick_button_handler)
{
    _joystick_button_handler = joystick_button_handler;
}

