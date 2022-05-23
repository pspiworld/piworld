#pragma once

#include "local_player.h"

void action_apply_bindings(LocalPlayer *local, char *bindings_str);
action_t get_action_from_name(char *name);

void action_view_up(LocalPlayer *local, Event *e);
void action_view_down(LocalPlayer *local, Event *e);
void action_view_left(LocalPlayer *local, Event *e);
void action_view_right(LocalPlayer *local, Event *e);
void action_move_forward(LocalPlayer *local, Event *e);
void action_move_back(LocalPlayer *local, Event *e);
void action_move_left(LocalPlayer *local, Event *e);
void action_move_right(LocalPlayer *local, Event *e);
void action_show_input_event(LocalPlayer *local, Event *e);
void action_add_block(LocalPlayer *local, Event *e);
void action_remove_block(LocalPlayer *local, Event *e);
void action_jump(LocalPlayer *local, Event *e);
void action_crouch(LocalPlayer *local, Event *e);
void action_next_item_in_hand(LocalPlayer *local, Event *e);
void action_previous_item_in_hand(LocalPlayer *local, Event *e);
void action_zoom(LocalPlayer *local, Event *e);
void action_ortho(LocalPlayer *local, Event *e);
void action_pick_item_in_hand(LocalPlayer *local, Event *e);
void action_fly_mode_toggle(LocalPlayer *local, Event *e);
void action_mode1(LocalPlayer *local, Event *e);
void action_mode2(LocalPlayer *local, Event *e);
void action_next_mode(LocalPlayer *local, Event *e);
void action_menu(LocalPlayer *local, Event *e);
void action_open_item_in_hand_menu(LocalPlayer *local, Event *e);
void action_move_left_right(LocalPlayer *local, Event *e);
void action_move_forward_back(LocalPlayer *local, Event *e);
void action_view_left_right(LocalPlayer *local, Event *e);
void action_view_up_down(LocalPlayer *local, Event *e);
void action_move_right_left(LocalPlayer *local, Event *e);
void action_move_back_forward(LocalPlayer *local, Event *e);
void action_view_right_left(LocalPlayer *local, Event *e);
void action_view_down_up(LocalPlayer *local, Event *e);
void action_undo(LocalPlayer *local, Event *e);
void action_set_mouse_absolute(LocalPlayer *local, Event *e);
void action_set_mouse_relative(LocalPlayer *local, Event *e);
void action_move_local_player_keyboard_and_mouse_to_next_active_player(LocalPlayer *local, Event *e);
void action_fullscreen_toggle(LocalPlayer *local, Event *e);
void action_observe_view_toggle(LocalPlayer *local, Event *e);
void action_picture_in_picture_observe_view_toggle(LocalPlayer *local, Event *e);
void action_open_action_command_line(LocalPlayer *local, Event *e);
void action_open_chat_command_line(LocalPlayer *local, Event *e);
void action_open_lua_command_line(LocalPlayer *local, Event *e);
void action_open_sign_command_line(LocalPlayer *local, Event *e);
void action_set_item_index_from_keysym_number(LocalPlayer *local, Event *e);
void action_primary(LocalPlayer *local, Event *e);
void action_secondary(LocalPlayer *local, Event *e);
void action_primary_mouse_action(LocalPlayer *local, Event *e);
void action_secondary_mouse_action(LocalPlayer *local, Event *e);
void action_tertiary_mouse_action(LocalPlayer *local, Event *e);
void action_open_menu_on_release(LocalPlayer *local, Event *e);
void action_crouch_toggle(LocalPlayer *local, Event *e);
void action_ortho_toggle(LocalPlayer *local, Event *e);
void action_zoom_toggle(LocalPlayer *local, Event *e);
void action_vt(LocalPlayer *local, Event *e);
void action_door(LocalPlayer *local, Event *e);

