// Some parts derived from GLFW - so those parts are licensed as:
//========================================================================
// GLFW 3.1 Linux - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2014 Jonas Ådahl <jadahl@gmail.com>
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

#pragma once

#define PG_JOYSTICK_1             0
#define PG_JOYSTICK_2             1
#define PG_JOYSTICK_3             2
#define PG_JOYSTICK_4             3
#define PG_JOYSTICK_5             4
#define PG_JOYSTICK_6             5
#define PG_JOYSTICK_7             6
#define PG_JOYSTICK_8             7
#define PG_JOYSTICK_9             8
#define PG_JOYSTICK_10            9
#define PG_JOYSTICK_11            10
#define PG_JOYSTICK_12            11
#define PG_JOYSTICK_13            12
#define PG_JOYSTICK_14            13
#define PG_JOYSTICK_15            14
#define PG_JOYSTICK_16            15
#define PG_JOYSTICK_LAST          PG_JOYSTICK_16

typedef struct PG_Joystick
{
    int             present;
    int             fd;
    float*          axes;
    int             axis_count;
    unsigned char*  buttons;
    int             button_count;
    char*           name;
    char*           path;
} PG_Joystick;

typedef void (*JoystickAxisHandler)(PG_Joystick *j, int j_num, int axis, float state);
typedef void (*JoystickButtonHandler)(PG_Joystick *j, int j_num, int button, int state);

int pg_init_joysticks(void);
void pg_terminate_joysticks(void);
void pg_set_joystick_button_handler(JoystickButtonHandler joystick_button_handler);
void pg_set_joystick_axis_handler(JoystickAxisHandler joystick_axis_handler);
void pg_poll_joystick_events(void);

