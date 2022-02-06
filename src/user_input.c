#include <string.h>
#include "action.h"
#include "client.h"
#include "chunk.h"
#include "chunks.h"
#include "local_player.h"
#include "local_player_command_line.h"
#include "local_players.h"
#include "pg.h"
#include "pw.h"
#include "stb_ds.h"
#include "user_input.h"
#include "x11_event_handler.h"

KeyBinding* default_key_bindings;

void insert_into_typing_buffer(LocalPlayer *local, unsigned char c) {
    size_t n = strlen(local->typing_buffer);
    if (n < MAX_TEXT_LENGTH - 1) {
        if (local->text_cursor != n) {
            // Shift text after the text cursor to the right
            memmove(local->typing_buffer + local->text_cursor + 1,
                    local->typing_buffer + local->text_cursor,
                    n - local->text_cursor);
        }
        local->typing_buffer[local->text_cursor] = c;
        local->typing_buffer[n + 1] = '\0';
        local->text_cursor += 1;
    }
}

void handle_key_press_typing(LocalPlayer *local, int mods, int keysym)
{
    switch (keysym) {
    case XK_Escape:
        local->typing = 0;
        local->history_position = NOT_IN_HISTORY;
        break;
    case XK_BackSpace:
        {
        size_t n = strlen(local->typing_buffer);
        if (n > 0 && local->text_cursor > local->typing_start) {
            if (local->text_cursor < n) {
                memmove(local->typing_buffer + local->text_cursor - 1,
                        local->typing_buffer + local->text_cursor,
                        n - local->text_cursor);
            }
            local->typing_buffer[n - 1] = '\0';
            local->text_cursor -= 1;
        }
        break;
        }
    case XK_Return:
        if (mods & ShiftMask) {
            insert_into_typing_buffer(local, '\r');
        } else {
            local->typing = 0;
            if (local->typing_buffer[0] == CRAFT_KEY_SIGN) {
                int x, y, z, face;
                if (hit_test_face(local->player, &x, &y, &z, &face)) {
                    set_sign(x, y, z, face, local->typing_buffer + 1);
                }
            } else if (local->typing_buffer[0] == CRAFT_KEY_COMMAND) {
                parse_command(local, local->typing_buffer, 1);
            } else if (local->typing_buffer[0] == PW_KEY_LUA) {
                pwlua_parse_line(local->lua_shell, local->typing_buffer);
            } else if (local->osk_open_for_menu == NULL) {
                client_talk(local->typing_buffer);
            }
            history_add(current_history(local), local->typing_buffer);
            local->history_position = NOT_IN_HISTORY;
        }
        break;
    case XK_Delete:
        {
        size_t n = strlen(local->typing_buffer);
        if (n > 0 && local->text_cursor < n) {
            memmove(local->typing_buffer + local->text_cursor,
                    local->typing_buffer + local->text_cursor + 1,
                    n - local->text_cursor);
            local->typing_buffer[n - 1] = '\0';
        }
        break;
        }
    case XK_Left:
        if (local->text_cursor > local->typing_start) {
            local->text_cursor -= 1;
        }
        break;
    case XK_Right:
        if (local->text_cursor < strlen(local->typing_buffer)) {
            local->text_cursor += 1;
        }
        break;
    case XK_Home:
        local->text_cursor = local->typing_start;
        break;
    case XK_End:
        local->text_cursor = strlen(local->typing_buffer);
        break;
    case XK_Up:
        history_previous(current_history(local), local->typing_buffer,
                         &local->history_position);
        local->text_cursor = strlen(local->typing_buffer);
        break;
    case XK_Down:
        history_next(current_history(local), local->typing_buffer,
                     &local->history_position);
        local->text_cursor = strlen(local->typing_buffer);
        break;
    default:
        if (keysym >= XK_space && keysym <= XK_asciitilde) {
            insert_into_typing_buffer(local, keysym);
        }
        break;
    }
}

void local_player_handle_key_press(LocalPlayer *local, int mods, int keysym)
{
    Event event = { 0 };
    event.type = KEYBOARD;
    event.button = keysym;
    event.mods = mods;
    event.state = 1;

    if (local->typing) {
        if (&local->osk == local->active_menu) {
            // Enable testing of OSK using keyboard with only the cursor keys,
            // return and escape.
            if (keysym == XK_Escape || keysym == XK_Up || keysym == XK_Down ||
                keysym == XK_Left || keysym == XK_Right ||
                keysym == XK_Return) {
                int r = menu_handle_key_press(local->active_menu, mods, keysym);
                handle_menu_event(local, local->active_menu, r);

                // extra Escape: close command line as well as OSK
                if (keysym == XK_Escape) {
                    handle_key_press_typing(local, mods, keysym);
                }
            } else { // handle key press as usual for command line
                handle_key_press_typing(local, mods, keysym);
            }
        } else {
            handle_key_press_typing(local, mods, keysym);
        }
        return;
    }
    if (local->active_menu) {
        int r = menu_handle_key_press(local->active_menu, mods, keysym);
        handle_menu_event(local, local->active_menu, r);
        open_osk_for_menu_line_edit(local, r);
        return;
    }

    action_t action = hmget(local->key_bindings, keysym);

    if (action == NULL) {
        action = hmget(default_key_bindings, keysym);
    }

    if (action != NULL) {
        action(local, &event);
        return;
    }
}

void local_player_handle_key_release(LocalPlayer *local, int keysym)
{
    Event event = { 0 };
    event.type = KEYBOARD;
    event.button = keysym;
    event.state = 0;

    action_t action = hmget(local->key_bindings, keysym);

    if (action == NULL) {
        action = hmget(default_key_bindings, keysym);
    }

    if (action != NULL) {
        action(local, &event);
        return;
    }
}

void local_player_handle_mouse_press(LocalPlayer *local, int b, int mods)
{
    Event event = { 0 };
    event.type = MOUSE_BUTTON;
    event.button = b;
    event.state = 1;
    event.mods = mods;

    if (local->active_menu) {
        return;
    }
    action_t action = NULL;
    if (local->mouse_bindings[b] != NULL) {
        action = local->mouse_bindings[b];
    } else if (b == 4) {
        action = &action_next_item_in_hand;
    } else if (b == 5) {
        action = &action_previous_item_in_hand;
    }

    if (action != NULL) {
        action(local, &event);
        return;
    }
}

void local_player_handle_mouse_release(LocalPlayer *local, int b, int mods)
{
    Event event = { 0 };
    event.type = MOUSE_BUTTON;
    event.button = b;
    event.state = 0;
    event.mods = mods;

    if (local->active_menu) {
        int r = menu_handle_mouse_release(local->active_menu, local->mouse_x,
                                          local->mouse_y, b);
        handle_menu_event(local, local->active_menu, r);
        if (local->keyboard_id == UNASSIGNED || config->always_use_osk) {
            open_osk_for_menu_line_edit(local, r);
        }
        return;
    }
    action_t action = NULL;
    if (local->mouse_bindings[b] != NULL) {
        action = local->mouse_bindings[b];
    } else if (b == 1) {
        action = &action_primary_mouse_action;
    } else if (b == 2) {
        action = &action_tertiary_mouse_action;
    } else if (b == 3) {
        action = &action_secondary_mouse_action;
    } else if (b == 4) {
        action = &action_next_item_in_hand;
    } else if (b == 5) {
        action = &action_previous_item_in_hand;
    } else {
        action = &action_open_menu_on_release;
    }

    if (action != NULL) {
        action(local, &event);
        return;
    }
}

void local_player_handle_mouse_motion(LocalPlayer *local, float x, float y)
{
    if (local->active_menu) {
        local->mouse_x -= x;
        local->mouse_y += y;
        if (local->mouse_x < 0) {
            local->mouse_x = 0;
        }
        if (local->mouse_x > local->view_width - 1) {
            local->mouse_x = local->view_width - 1;
        }
        if (local->mouse_y < -(MOUSE_CURSOR_SIZE - 1)) {
            local->mouse_y = -(MOUSE_CURSOR_SIZE - 1);
        }
        if (local->mouse_y > local->view_height - MOUSE_CURSOR_SIZE) {
            local->mouse_y = local->view_height - MOUSE_CURSOR_SIZE;
        }
        menu_handle_mouse(local->active_menu, local->mouse_x, local->mouse_y);
        return;
    }
    State *s = &local->player->state;
    float m = 0.0025;
    if (local->zoom_is_pressed) {
        m = 0.0005;
    }
    s->rx -= x * m;
    s->ry += y * m;
    if (s->rx < 0) {
        s->rx += RADIANS(360);
    }
    if (s->rx >= RADIANS(360)) {
        s->rx -= RADIANS(360);
    }
    s->ry = MAX(s->ry, -RADIANS(90));
    s->ry = MIN(s->ry, RADIANS(90));
}

void local_player_handle_joystick_axis(LocalPlayer *local, PG_Joystick *j,
    int axis, float value)
{
    Event event = { 0 };
    event.type = JOYSTICK_AXIS;
    event.button = axis;
    event.x = value;
    Menu *menu = local->active_menu;
    action_t action;

    if (menu) {
        action = local->gamepad_axes_bindings[axis];
        if (action == &action_move_forward_back) {
            if (value > 0.5) {
                menu_highlight_up(menu);
            } else if (value < -0.5) {
                menu_highlight_down(menu);
            }
        } else if (action == &action_move_left_right) {
            if (value > 0.5) {
                menu_highlight_right(menu);
            } else if (value < -0.5) {
                menu_highlight_left(menu);
            }
        } else {
            menu_handle_joystick_axis(local->active_menu, axis, value);
        }
        return;
    }
    static action_t no_stick_gamepad[] = {
        &action_move_left_right,
        &action_move_forward_back,
    };
    static const int no_stick_gamepad_count = sizeof(no_stick_gamepad) / sizeof(action_t);

    static action_t two_stick_gamepad[] = {
        &action_move_left_right,
        &action_move_forward_back,
        &action_view_up_down,
        &action_view_left_right,
        &action_move_left_right,
        &action_move_forward_back,
    };
    static const int two_stick_gamepad_count = sizeof(two_stick_gamepad) / sizeof(action_t);

    action = NULL;

    if (local->gamepad_axes_bindings[axis] != NULL) {
        action = local->gamepad_axes_bindings[axis];
    } else if (j->axis_count < 4) {
        if (axis < no_stick_gamepad_count) {
            action = no_stick_gamepad[axis];
        }
    } else {
        if (axis < two_stick_gamepad_count) {
            action = two_stick_gamepad[axis];
        }
    }

    if (action != NULL) {
        action(local, &event);
        return;
    }
}

void local_player_handle_joystick_button(LocalPlayer *local, PG_Joystick *j,
    int button, int state)
{
    Event event = { 0 };
    event.type = JOYSTICK_BUTTON;
    event.button = button;
    event.state = state;
    Menu *menu = local->active_menu;
    action_t action;

    if (menu) {
        int r;
        action = local->gamepad_bindings[button];
        if (action == &action_move_forward && state == 1) {
            menu_highlight_up(menu);
        } else if (action == &action_move_back && state == 1) {
            menu_highlight_down(menu);
        } else if (action == &action_move_left && state == 1) {
            menu_highlight_left(menu);
        } else if (action == &action_move_right && state == 1) {
            menu_highlight_right(menu);
        } else {
            r = menu_handle_joystick_button(local->active_menu, button, state);
            handle_menu_event(local, local->active_menu, r);
            open_osk_for_menu_line_edit(local, r);
        }
        return;
    }

    static action_t no_stick_gamepad[] = {
        &action_view_up,
        &action_view_right,
        &action_view_down,
        &action_view_left,
        &action_mode2,
        &action_mode1,
        NULL,
        NULL,
        &action_next_mode,
        &action_menu,
    };
    static const int no_stick_gamepad_count = sizeof(no_stick_gamepad) / sizeof(action_t);

    static action_t two_stick_gamepad[] = {
        &action_next_item_in_hand,
        &action_fly_mode_toggle,
        &action_previous_item_in_hand,
        &action_pick_item_in_hand,
        &action_crouch,
        &action_remove_block,
        &action_jump,
        &action_add_block,
        &action_zoom,
        &action_menu,
        &action_ortho,
        &action_zoom,
    };
    static const int two_stick_gamepad_count = sizeof(two_stick_gamepad) / sizeof(action_t);

    action = NULL;

    if (local->gamepad_bindings[button] != NULL) {
        action = local->gamepad_bindings[button];
    } else if (j->axis_count < 4) {
        if (button < no_stick_gamepad_count) {
            action = no_stick_gamepad[button];
        }
    } else {
        if (button < two_stick_gamepad_count) {
            action = two_stick_gamepad[button];
        }
    }

    if (action != NULL) {
        action(local, &event);
        return;
    }
}

void user_input_init(void)
{
    // Setup default key and other bindings
    hmput(default_key_bindings, XK_Up, &action_view_up);
    hmput(default_key_bindings, XK_Down, &action_view_down);
    hmput(default_key_bindings, XK_Left, &action_view_left);
    hmput(default_key_bindings, XK_Right, &action_view_right);
    hmput(default_key_bindings, XK_w, &action_move_forward);
    hmput(default_key_bindings, XK_a, &action_move_left);
    hmput(default_key_bindings, XK_s, &action_move_back);
    hmput(default_key_bindings, XK_d, &action_move_right);
    hmput(default_key_bindings, XK_Escape, &action_menu);
    hmput(default_key_bindings, XK_i, &action_open_item_in_hand_menu);
    hmput(default_key_bindings, XK_F1, &action_set_mouse_absolute);
    hmput(default_key_bindings, XK_F2, &action_set_mouse_relative);
    hmput(default_key_bindings, XK_F8, &action_move_local_player_keyboard_and_mouse_to_next_active_player);
    hmput(default_key_bindings, XK_F11, &action_fullscreen_toggle);
    hmput(default_key_bindings, XK_space, &action_jump);
    hmput(default_key_bindings, XK_c, &action_crouch);
    hmput(default_key_bindings, XK_z, &action_zoom);
    hmput(default_key_bindings, XK_f, &action_ortho);
    hmput(default_key_bindings, XK_Tab, &action_fly_mode_toggle);
    hmput(default_key_bindings, XK_1, &action_set_item_index_from_keysym_number);
    hmput(default_key_bindings, XK_2, &action_set_item_index_from_keysym_number);
    hmput(default_key_bindings, XK_3, &action_set_item_index_from_keysym_number);
    hmput(default_key_bindings, XK_4, &action_set_item_index_from_keysym_number);
    hmput(default_key_bindings, XK_5, &action_set_item_index_from_keysym_number);
    hmput(default_key_bindings, XK_6, &action_set_item_index_from_keysym_number);
    hmput(default_key_bindings, XK_7, &action_set_item_index_from_keysym_number);
    hmput(default_key_bindings, XK_8, &action_set_item_index_from_keysym_number);
    hmput(default_key_bindings, XK_9, &action_set_item_index_from_keysym_number);
    hmput(default_key_bindings, XK_0, &action_set_item_index_from_keysym_number);
    hmput(default_key_bindings, XK_e, &action_next_item_in_hand);
    hmput(default_key_bindings, XK_r, &action_previous_item_in_hand);
    hmput(default_key_bindings, XK_g, &action_pick_item_in_hand);
    hmput(default_key_bindings, XK_o, &action_observe_view_toggle);
    hmput(default_key_bindings, XK_p, &action_picture_in_picture_observe_view_toggle);
    hmput(default_key_bindings, XK_t, &action_open_chat_command_line);
    hmput(default_key_bindings, XK_u, &action_undo);
    hmput(default_key_bindings, XK_slash, &action_open_action_command_line);
    hmput(default_key_bindings, XK_dollar, &action_open_lua_command_line);
    hmput(default_key_bindings, XK_grave, &action_open_sign_command_line);
    hmput(default_key_bindings, XK_Return, &action_primary);
}

void user_input_deinit(void)
{
    hmfree(default_key_bindings);
}
