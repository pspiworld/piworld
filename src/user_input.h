#pragma once

#include "pg_joystick.h"

void local_player_handle_key_press(LocalPlayer *local, int mods, int keysym);
void local_player_handle_key_release(LocalPlayer *local, int keysym);
void local_player_handle_mouse_release(LocalPlayer *local, int b, int mods);
void local_player_handle_mouse_motion(LocalPlayer *local, float x, float y);
void local_player_handle_joystick_axis(LocalPlayer *local, PG_Joystick *j,
    int axis, float value);
void local_player_handle_joystick_button(LocalPlayer *local, PG_Joystick *j,
    int button, int state);

