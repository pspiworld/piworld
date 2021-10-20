#pragma once

#include "local_player.h"
#include "pg_joystick.h"

extern LocalPlayer local_players[MAX_LOCAL_PLAYERS];

void local_players_init(void);
LocalPlayer *get_local_player(int p);
LocalPlayer* player_for_keyboard(int keyboard_id);
LocalPlayer* player_for_mouse(int mouse_id);
LocalPlayer* player_for_joystick(int joystick_id);
void set_players_view_size(int w, int h);
void move_players_to_empty_block(void);
int keyboard_player_count(void);
void move_local_player_keyboard_and_mouse_to_next_active_player(
    LocalPlayer *p);
void handle_mouse_motion(int mouse_id, float x, float y);
void handle_key_press(int keyboard_id, int mods, int keysym);
void handle_key_release(int keyboard_id, int keysym);
void handle_joystick_axis(PG_Joystick *j, int j_num, int axis, float value);
void handle_joystick_button(PG_Joystick *j, int j_num, int button, int state);
void handle_mouse_release(int mouse_id, int b);
void handle_window_close(void);
void handle_focus_out(void);

