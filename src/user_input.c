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
#include "pwlua_startup.h"
#include "stb_ds.h"
#include "user_input.h"
#include "vt.h"
#include "x11_event_handler.h"

KeyBinding* default_key_bindings;
KeyBinding* default_no_stick_gamepad_bindings;
KeyBinding* default_two_stick_gamepad_bindings;
KeyBinding* default_no_stick_gamepad_axis_bindings;
KeyBinding* default_two_stick_gamepad_axis_bindings;

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
    if (local->vt_open) {
        vt_handle_key_press(local->pwt, mods, keysym);
        return;
    }

    switch (keysym) {
    case XK_Escape:
        local->typing = GameFocus;
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
            local->typing = GameFocus;
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

void call_action(LocalPlayer *local, Action *action, Event* event)
{
    if (action != NULL) {
        if (action->function != NULL) {
            action->function(local, event);
        } else {
            pwlua_startup_call_action(local, action->name, event);
        }
    }
}

void local_player_handle_key_press(LocalPlayer *local, int mods, int keysym)
{
    Event event = { 0 };
    event.type = KEYBOARD;
    event.button = keysym;
    event.mods = mods;
    event.state = 1;

    if (local->typing != GameFocus) {
        if (local->osk == local->active_menu) {
            // Enable testing of OSK using keyboard with only the cursor keys,
            // return and escape.
            if (keysym == XK_Escape || keysym == XK_Up || keysym == XK_Down ||
                keysym == XK_Left || keysym == XK_Right ||
                keysym == XK_Return) {
                int r = menu_handle_key_press(local->active_menu, mods, keysym);
                handle_menu_event(local, local->active_menu, r);

                // extra Escape: close command line as well as OSK
                if (keysym == XK_Escape) {
                    if (local->typing != VTFocus) {
                        handle_key_press_typing(local, mods, keysym);
                    }
                }
            } else { // handle key press as usual for command line
                handle_key_press_typing(local, mods, keysym);
            }
        } else if (local->typing == VTFocus) {
            Action *action = hmget(local->key_bindings, keysym);
            if (action == NULL) {
                action = hmget(default_key_bindings, keysym);
            }
            if (action->function == &action_vt) {
                if (!local->vt_open) {
                    local->vt_open = 1;
                    local->typing = VTFocus;
                } else {
                    local->vt_open = 0;
                    local->typing = GameFocus;
                }
            } else {
                vt_handle_key_press(local->pwt, mods, keysym);
            }
        } else {  // CommandLineFocus
            handle_key_press_typing(local, mods, keysym);
        }
        return;
    }
    if (local->active_menu) {
        Menu *menu = local->active_menu;
        int r = menu_handle_key_press(menu, mods, keysym);
        handle_menu_event(local, menu, r);
        if (menu == local->active_menu) {
            open_osk_for_menu_line_edit(local, r);
        }
        return;
    }

    Action *action = hmget(local->key_bindings, keysym);

    if (action == NULL) {
        action = hmget(default_key_bindings, keysym);
    }

    call_action(local, action, &event);
}

void local_player_handle_key_release(LocalPlayer *local, int keysym)
{
    Event event = { 0 };
    event.type = KEYBOARD;
    event.button = keysym;
    event.state = 0;

    Action *action = hmget(local->key_bindings, keysym);

    if (action == NULL) {
        action = hmget(default_key_bindings, keysym);
    }

    call_action(local, action, &event);
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
    Action *action = NULL;
    if (local->mouse_bindings[b] != NULL) {
        action = local->mouse_bindings[b];
    } else if (b == 4) {
        action = get_action_from_name("next_item_in_hand");
    } else if (b == 5) {
        action = get_action_from_name("previous_item_in_hand");
    }

    call_action(local, action, &event);
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
        if (use_osk(local)) {
            open_osk_for_menu_line_edit(local, r);
        }
        return;
    }
    Action *action = NULL;
    if (local->mouse_bindings[b] != NULL) {
        action = local->mouse_bindings[b];
    } else if (b == 1) {
        action = get_action_from_name("primary_mouse_action");
    } else if (b == 2) {
        action = get_action_from_name("tertiary_mouse_action");
    } else if (b == 3) {
        action = get_action_from_name("secondary_mouse_action");
    } else if (b == 4) {
        action = get_action_from_name("next_item_in_hand");
    } else if (b == 5) {
        action = get_action_from_name("previous_item_in_hand");
    } else {
        action = get_action_from_name("open_menu_on_release");
    }

    call_action(local, action, &event);
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
    Action *action;

    if (menu) {
        action = local->gamepad_axes_bindings[axis];
        menu_handle_joystick_axis(local->active_menu, axis, value);
        return;
    }

    action = NULL;

    if (local->gamepad_axes_bindings[axis] != NULL) {
        action = local->gamepad_axes_bindings[axis];
    } else if (j->axis_count < 4) {
        action = hmget(default_no_stick_gamepad_axis_bindings, axis);
    } else {
        action = hmget(default_two_stick_gamepad_axis_bindings, axis);
    }

    call_action(local, action, &event);
}

void local_player_handle_joystick_button(LocalPlayer *local, PG_Joystick *j,
    int button, int state)
{
    Event event = { 0 };
    event.type = JOYSTICK_BUTTON;
    event.button = button;
    event.state = state;
    Menu *menu = local->active_menu;
    Action *action;

    if (menu) {
        int r;
        action = local->gamepad_bindings[button];
        if (action) {
            if (action->function == &action_move_forward && state == 1) {
                menu_highlight_up(menu);
            } else if (action->function == &action_move_back && state == 1) {
                menu_highlight_down(menu);
            } else if (action->function == &action_move_left && state == 1) {
                menu_highlight_left(menu);
            } else if (action->function == &action_move_right && state == 1) {
                menu_highlight_right(menu);
            } else {
                r = menu_handle_joystick_button(local->active_menu, button, state);
                handle_menu_event(local, local->active_menu, r);
                open_osk_for_menu_line_edit(local, r);
            }
        } else {
            r = menu_handle_joystick_button(local->active_menu, button, state);
            handle_menu_event(local, local->active_menu, r);
            open_osk_for_menu_line_edit(local, r);
        }
        return;
    }
    action = NULL;

    if (local->gamepad_bindings[button] != NULL) {
        action = local->gamepad_bindings[button];
    } else if (j->axis_count < 4) {
        action = hmget(default_no_stick_gamepad_bindings, button);
    } else {
        action = hmget(default_two_stick_gamepad_bindings, button);
    }

    call_action(local, action, &event);
}

void user_input_init(void)
{
    // Setup default key and other bindings
    hmput(default_key_bindings, XK_Up, get_action_from_name("view_up"));
    hmput(default_key_bindings, XK_Down, get_action_from_name("view_down"));
    hmput(default_key_bindings, XK_Left, get_action_from_name("view_left"));
    hmput(default_key_bindings, XK_Right, get_action_from_name("view_right"));
    hmput(default_key_bindings, XK_w, get_action_from_name("move_forward"));
    hmput(default_key_bindings, XK_a, get_action_from_name("move_left"));
    hmput(default_key_bindings, XK_s, get_action_from_name("move_back"));
    hmput(default_key_bindings, XK_d, get_action_from_name("move_right"));
    hmput(default_key_bindings, XK_Escape, get_action_from_name("menu"));
    hmput(default_key_bindings, XK_Menu, get_action_from_name("menu"));
    hmput(default_key_bindings, XK_i, get_action_from_name("open_item_in_hand_menu"));
    hmput(default_key_bindings, XK_F1, get_action_from_name("set_mouse_absolute"));
    hmput(default_key_bindings, XK_F2, get_action_from_name("set_mouse_relative"));
    hmput(default_key_bindings, XK_F8, get_action_from_name("move_local_player_keyboard_and_mouse_to_next_active_player"));
    hmput(default_key_bindings, XK_F11, get_action_from_name("fullscreen_toggle"));
    hmput(default_key_bindings, XK_space, get_action_from_name("jump"));
    hmput(default_key_bindings, XK_c, get_action_from_name("crouch"));
    hmput(default_key_bindings, XK_z, get_action_from_name("zoom"));
    hmput(default_key_bindings, XK_f, get_action_from_name("ortho"));
    hmput(default_key_bindings, XK_Tab, get_action_from_name("fly_mode_toggle"));
    hmput(default_key_bindings, XK_1, get_action_from_name("set_item_index_from_keysym_number"));
    hmput(default_key_bindings, XK_2, get_action_from_name("set_item_index_from_keysym_number"));
    hmput(default_key_bindings, XK_3, get_action_from_name("set_item_index_from_keysym_number"));
    hmput(default_key_bindings, XK_4, get_action_from_name("set_item_index_from_keysym_number"));
    hmput(default_key_bindings, XK_5, get_action_from_name("set_item_index_from_keysym_number"));
    hmput(default_key_bindings, XK_6, get_action_from_name("set_item_index_from_keysym_number"));
    hmput(default_key_bindings, XK_7, get_action_from_name("set_item_index_from_keysym_number"));
    hmput(default_key_bindings, XK_8, get_action_from_name("set_item_index_from_keysym_number"));
    hmput(default_key_bindings, XK_9, get_action_from_name("set_item_index_from_keysym_number"));
    hmput(default_key_bindings, XK_0, get_action_from_name("set_item_index_from_keysym_number"));
    hmput(default_key_bindings, XK_e, get_action_from_name("next_item_in_hand"));
    hmput(default_key_bindings, XK_r, get_action_from_name("previous_item_in_hand"));
    hmput(default_key_bindings, XK_g, get_action_from_name("pick_item_in_hand"));
    hmput(default_key_bindings, XK_o, get_action_from_name("observe_view_toggle"));
    hmput(default_key_bindings, XK_p, get_action_from_name("picture_in_picture_observe_view_toggle"));
    hmput(default_key_bindings, XK_t, get_action_from_name("open_chat_command_line"));
    hmput(default_key_bindings, XK_u, get_action_from_name("undo"));
    hmput(default_key_bindings, XK_slash, get_action_from_name("open_action_command_line"));
    hmput(default_key_bindings, XK_dollar, get_action_from_name("open_lua_command_line"));
    hmput(default_key_bindings, XK_grave, get_action_from_name("open_sign_command_line"));
    hmput(default_key_bindings, XK_Return, get_action_from_name("primary"));

    hmput(default_no_stick_gamepad_bindings, 0, get_action_from_name("view_up"));
    hmput(default_no_stick_gamepad_bindings, 1, get_action_from_name("view_right"));
    hmput(default_no_stick_gamepad_bindings, 2, get_action_from_name("view_down"));
    hmput(default_no_stick_gamepad_bindings, 3, get_action_from_name("view_left"));
    hmput(default_no_stick_gamepad_bindings, 4, get_action_from_name("mode2"));
    hmput(default_no_stick_gamepad_bindings, 5, get_action_from_name("mode1"));
    hmput(default_no_stick_gamepad_bindings, 8, get_action_from_name("next_mode"));
    hmput(default_no_stick_gamepad_bindings, 9, get_action_from_name("menu"));

    hmput(default_two_stick_gamepad_bindings, 0, get_action_from_name("next_item_in_hand"));
    hmput(default_two_stick_gamepad_bindings, 1, get_action_from_name("fly_mode_toggle"));
    hmput(default_two_stick_gamepad_bindings, 2, get_action_from_name("previous_item_in_hand"));
    hmput(default_two_stick_gamepad_bindings, 3, get_action_from_name("pick_item_in_hand"));
    hmput(default_two_stick_gamepad_bindings, 4, get_action_from_name("crouch"));
    hmput(default_two_stick_gamepad_bindings, 5, get_action_from_name("remove_block"));
    hmput(default_two_stick_gamepad_bindings, 6, get_action_from_name("jump"));
    hmput(default_two_stick_gamepad_bindings, 7, get_action_from_name("add_block"));
    hmput(default_two_stick_gamepad_bindings, 8, get_action_from_name("zoom"));
    hmput(default_two_stick_gamepad_bindings, 9, get_action_from_name("menu"));
    hmput(default_two_stick_gamepad_bindings, 10, get_action_from_name("ortho"));
    hmput(default_two_stick_gamepad_bindings, 11, get_action_from_name("zoom"));

    hmput(default_no_stick_gamepad_axis_bindings, 0, get_action_from_name("move_left_right"));
    hmput(default_no_stick_gamepad_axis_bindings, 1, get_action_from_name("move_forward_back"));

    hmput(default_two_stick_gamepad_axis_bindings, 0, get_action_from_name("move_left_right"));
    hmput(default_two_stick_gamepad_axis_bindings, 1, get_action_from_name("move_forward_back"));
    hmput(default_two_stick_gamepad_axis_bindings, 2, get_action_from_name("view_up_down"));
    hmput(default_two_stick_gamepad_axis_bindings, 3, get_action_from_name("view_left_right"));
    hmput(default_two_stick_gamepad_axis_bindings, 4, get_action_from_name("move_left_right"));
    hmput(default_two_stick_gamepad_axis_bindings, 5, get_action_from_name("move_forward_back"));
}

void user_input_deinit(void)
{
    hmfree(default_key_bindings);
    hmfree(default_no_stick_gamepad_bindings);
    hmfree(default_two_stick_gamepad_bindings);
    hmfree(default_no_stick_gamepad_axis_bindings);
    hmfree(default_two_stick_gamepad_axis_bindings);
}
