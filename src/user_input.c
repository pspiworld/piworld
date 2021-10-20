#include <math.h>
#include <string.h>
#include "client.h"
#include "chunk.h"
#include "chunks.h"
#include "local_player.h"
#include "local_player_command_line.h"
#include "local_players.h"
#include "pg.h"
#include "pw.h"
#include "user_input.h"
#include "x11_event_handler.h"

#define DEADZONE 0.0

#define CROUCH_JUMP 0
#define REMOVE_ADD 1
#define CYCLE_DOWN_UP 2
#define FLY_PICK 3
#define ZOOM_ORTHO 4
#define SHOULDER_BUTTON_MODE_COUNT 5
const char *shoulder_button_modes[SHOULDER_BUTTON_MODE_COUNT] = {
    "Crouch/Jump",
    "Remove/Add",
    "Previous/Next",
    "Fly/Pick",
    "Zoom/Ortho"
};

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
            } else {
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
    if (local->typing) {
        handle_key_press_typing(local, mods, keysym);
        return;
    }
    if (local->active_menu) {
        int r = menu_handle_key_press(local->active_menu, mods, keysym);
        handle_menu_event(local, local->active_menu, r);
        return;
    }
    switch (keysym) {
    case XK_w: case XK_W:
        local->forward_is_pressed = 1;
        local->movement_speed_forward_back = 1;
        break;
    case XK_s: case XK_S:
        local->back_is_pressed = 1;
        local->movement_speed_forward_back = 1;
        break;
    case XK_a: case XK_A:
        local->left_is_pressed = 1;
        local->movement_speed_left_right = 1;
        break;
    case XK_d: case XK_D:
        local->right_is_pressed = 1;
        local->movement_speed_left_right = 1;
        break;
    case XK_Escape:
        if (local->active_menu) {
            close_menu(local);
        } else {
            open_menu(local, &local->menu);
        }
        break;
    case XK_i: case XK_I:
        open_menu(local, &local->menu_item_in_hand);
        break;
    case XK_F1:
        set_mouse_absolute();
        break;
    case XK_F2:
        set_mouse_relative();
        break;
    case XK_F8:
        move_local_player_keyboard_and_mouse_to_next_active_player(local);
        break;
    case XK_F11:
        pg_toggle_fullscreen();
        break;
    case XK_Up:
        local->view_up_is_pressed = 1;
        local->view_speed_up_down = 1;
        break;
    case XK_Down:
        local->view_down_is_pressed = 1;
        local->view_speed_up_down = 1;
        break;
    case XK_Left:
        local->view_left_is_pressed = 1;
        local->view_speed_left_right = 1;
        break;
    case XK_Right:
        local->view_right_is_pressed = 1;
        local->view_speed_left_right = 1;
        break;
    case XK_space:
        local->jump_is_pressed = 1;
        break;
    case XK_c: case XK_C:
        local->crouch_is_pressed = 1;
        break;
    case XK_z: case XK_Z:
        local->zoom_is_pressed = 1;
        break;
    case XK_f: case XK_F:
        local->ortho_is_pressed = 1;
        break;
    case XK_Tab:
        if (!mods) {
            local->flying = !local->flying;
        }
        break;
    case XK_1: case XK_2: case XK_3: case XK_4: case XK_5: case XK_6:
    case XK_7: case XK_8: case XK_9:
        local->item_index = keysym - XK_1;
        break;
    case XK_0:
        local->item_index = 9;
        break;
    case XK_e: case XK_E:
        cycle_item_in_hand_up(local);
        break;
    case XK_r: case XK_R:
        cycle_item_in_hand_down(local);
        break;
    case XK_g: case XK_G:
        set_item_in_hand_to_item_under_crosshair(local);
        break;
    case XK_o: case XK_O:
        toggle_observe_view(local);
        break;
    case XK_p: case XK_P:
        toggle_picture_in_picture_observe_view(local);
        break;
    case XK_t: case XK_T:
        local->typing = 1;
        local->typing_buffer[0] = '\0';
        local->typing_start = local->typing_history[CHAT_HISTORY].line_start;
        local->text_cursor = local->typing_start;
        break;
    case XK_u: case XK_U:
        if (local->has_undo_block == 1) {
            UndoBlock *b = &local->undo_block;
            int bp = chunked(b->x);
            int bq = chunked(b->z);
            set_block(b->x, b->y, b->z, b->texture);
            set_extra(b->x, b->y, b->z, b->extra);
            set_light(bp, bq, b->x, b->y, b->z, b->light);
            set_shape(b->x, b->y, b->z, b->shape);
            set_transform(b->x, b->y, b->z, b->transform);
            if (b->has_sign) {
                SignList *signs = &b->signs;
                for (size_t i = 0; i < signs->size; i++) {
                    Sign *e = signs->data + i;
                    set_sign(e->x, e->y, e->z, e->face, e->text);
                }
                sign_list_free(signs);
                b->has_sign = 0;
            }
            local->has_undo_block = 0;
        }
        break;
    case XK_slash:
        local->typing = 1;
        local->typing_buffer[0] = CRAFT_KEY_COMMAND;
        local->typing_buffer[1] = '\0';
        local->typing_start = local->typing_history[COMMAND_HISTORY].line_start;
        local->text_cursor = local->typing_start;
        break;
    case XK_dollar:
        local->typing = 1;
        local->typing_buffer[0] = PW_KEY_LUA;
        local->typing_buffer[1] = '\0';
        local->typing_start = local->typing_history[LUA_HISTORY].line_start;
        local->text_cursor = local->typing_start;
        break;
    case XK_grave:
        local->typing = 1;
        local->typing_buffer[0] = CRAFT_KEY_SIGN;
        local->typing_buffer[1] = '\0';
        int x, y, z, face;
        if (hit_test_face(local->player, &x, &y, &z, &face)) {
            const unsigned char *existing_sign = get_sign(
                chunked(x), chunked(z), x, y, z, face);
            if (existing_sign) {
                strncpy(local->typing_buffer + 1, (char *)existing_sign,
                        MAX_TEXT_LENGTH - 1);
                local->typing_buffer[MAX_TEXT_LENGTH - 1] = '\0';
            }
        }
        local->typing_start = local->typing_history[SIGN_HISTORY].line_start;
        local->text_cursor = local->typing_start;
        break;
    case XK_Return:
        if (mods & ControlMask) {
            set_block_under_crosshair(local);
        } else {
            clear_block_under_crosshair(local);
        }
        break;
    }
}

void local_player_handle_key_release(LocalPlayer *local, int keysym)
{
    switch (keysym) {
    case XK_w: case XK_W:
        local->forward_is_pressed = 0;
        break;
    case XK_s: case XK_S:
        local->back_is_pressed = 0;
        break;
    case XK_a: case XK_A:
        local->left_is_pressed = 0;
        break;
    case XK_d: case XK_D:
        local->right_is_pressed = 0;
        break;
    case XK_space:
        local->jump_is_pressed = 0;
        break;
    case XK_c: case XK_C:
        local->crouch_is_pressed = 0;
        break;
    case XK_z: case XK_Z:
        local->zoom_is_pressed = 0;
        break;
    case XK_f: case XK_F:
        local->ortho_is_pressed = 0;
        break;
    case XK_Up:
        local->view_up_is_pressed = 0;
        break;
    case XK_Down:
        local->view_down_is_pressed = 0;
        break;
    case XK_Left:
        local->view_left_is_pressed = 0;
        break;
    case XK_Right:
        local->view_right_is_pressed = 0;
        break;
    }
}

void local_player_handle_mouse_release(LocalPlayer *local, int b, int mods)
{
    if (local->active_menu) {
        int r = menu_handle_mouse_release(local->active_menu, local->mouse_x,
                                            local->mouse_y, b);
        handle_menu_event(local, local->active_menu, r);
        return;
    }
    if (b == 1) {
        if (mods & ControlMask) {
            set_block_under_crosshair(local);
        } else {
            clear_block_under_crosshair(local);
        }
    } else if (b == 2) {
        open_menu_for_item_under_crosshair(local);
    } else if (b == 3) {
        if (mods & ControlMask) {
            on_light(local);
        } else {
            set_block_under_crosshair(local);
        }
    } else if (b == 4) {
        cycle_item_in_hand_up(local);
    } else if (b == 5) {
        cycle_item_in_hand_down(local);
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
    if (local->active_menu) {
        menu_handle_joystick_axis(local->active_menu, axis, value);
        return;
    }

    if (j->axis_count < 4) {
        if (axis == 0) {
            local->left_is_pressed = (value < 0) ? 1 : 0;
            local->right_is_pressed = (value > 0) ? 1 : 0;
            local->movement_speed_left_right = fabs(value);
        } else if (axis == 1) {
            local->forward_is_pressed = (value < 0) ? 1 : 0;
            local->back_is_pressed = (value > 0) ? 1 : 0;
            local->movement_speed_forward_back = fabs(value);
        }
    } else {
        if (axis == 0) {
            local->movement_speed_left_right = fabs(value) * 2.0;
            local->left_is_pressed = (value < -DEADZONE);
            local->right_is_pressed = (value > DEADZONE);
        } else if (axis == 1) {
            local->movement_speed_forward_back = fabs(value) * 2.0;
            local->forward_is_pressed = (value < -DEADZONE);
            local->back_is_pressed = (value > DEADZONE);
        } else if (axis == 2) {
            local->view_speed_up_down = fabs(value) * 2.0;
            local->view_up_is_pressed = (value < -DEADZONE);
            local->view_down_is_pressed = (value > DEADZONE);
        } else if (axis == 3) {
            local->view_speed_left_right = fabs(value) * 2.0;
            local->view_left_is_pressed = (value < -DEADZONE);
            local->view_right_is_pressed = (value > DEADZONE);
        } else if (axis == 4) {
            local->movement_speed_left_right = fabs(value);
            local->left_is_pressed = (value < 0) ? 1 : 0;
            local->right_is_pressed = (value > 0) ? 1 : 0;
        } else if (axis == 5) {
            local->movement_speed_forward_back = fabs(value);
            local->forward_is_pressed = (value < 0) ? 1 : 0;
            local->back_is_pressed = (value > 0) ? 1 : 0;
        }
    }
}

void local_player_handle_joystick_button(LocalPlayer *local, PG_Joystick *j,
    int button, int state)
{
    if (local->active_menu) {
        int r = menu_handle_joystick_button(local->active_menu, button, state);
        handle_menu_event(local, local->active_menu, r);
        return;
    }

    if (j->axis_count < 4) {
        if (button == 0) {
            local->view_up_is_pressed = state;
            local->view_speed_up_down = 1;
        } else if (button == 2) {
            local->view_down_is_pressed = state;
            local->view_speed_up_down = 1;
        } else if (button == 3) {
            local->view_left_is_pressed = state;
            local->view_speed_left_right = 1;
        } else if (button == 1) {
            local->view_right_is_pressed = state;
            local->view_speed_left_right = 1;
        } else if (button == 5) {
            switch (local->shoulder_button_mode) {
                case CROUCH_JUMP:
                    local->jump_is_pressed = state;
                    break;
                case REMOVE_ADD:
                    if (state) {
                        set_block_under_crosshair(local);
                    }
                    break;
                case CYCLE_DOWN_UP:
                    if (state) {
                        cycle_item_in_hand_up(local);
                    }
                    break;
                case FLY_PICK:
                    if (state) {
                        set_item_in_hand_to_item_under_crosshair(local);
                    }
                    break;
                case ZOOM_ORTHO:
                    local->ortho_is_pressed = state;
                    break;
            }
        } else if (button == 4) {
           switch (local->shoulder_button_mode) {
                case CROUCH_JUMP:
                    local->crouch_is_pressed = state;
                    break;
                case REMOVE_ADD:
                    if (state) {
                        clear_block_under_crosshair(local);
                    }
                    break;
                case CYCLE_DOWN_UP:
                    if (state) {
                        cycle_item_in_hand_down(local);
                    }
                    break;
                case FLY_PICK:
                    if (state) {
                        local->flying = !local->flying;
                    }
                    break;
                case ZOOM_ORTHO:
                    local->zoom_is_pressed = state;
                    break;
            }
        } else if (button == 8) {
            if (state) {
                local->shoulder_button_mode += 1;
                local->shoulder_button_mode %= SHOULDER_BUTTON_MODE_COUNT;
                add_message(local->player->id,
                            shoulder_button_modes[local->shoulder_button_mode]);
            }
        } else if (button == 9) {
            if (state) {
                if (local->active_menu) {
                    close_menu(local);
                } else {
                    open_menu(local, &local->menu);
                }
            }
        }
    } else {
        if (button == 0) {
            if (state) {
                cycle_item_in_hand_up(local);
            }
        } else if (button == 2) {
            if (state) {
                cycle_item_in_hand_down(local);
            }
        } else if (button == 3) {
            if (state) {
                set_item_in_hand_to_item_under_crosshair(local);
            }
        } else if (button == 1) {
            if (state) {
                local->flying = !local->flying;
            }
        } else if (button == 5) {
            if (state) {
                clear_block_under_crosshair(local);
            }
        } else if (button == 4) {
            local->crouch_is_pressed = state;
        } else if (button == 6) {
            local->jump_is_pressed = state;
        } else if (button == 7) {
            if (state) {
                set_block_under_crosshair(local);
            }
        } else if (button == 8) {
            local->zoom_is_pressed = state;
        } else if (button == 9) {
            if (state) {
                if (local->active_menu) {
                    close_menu(local);
                } else {
                    open_menu(local, &local->menu);
                }
            }
        } else if (button == 10) {
            local->ortho_is_pressed = state;
        } else if (button == 11) {
            local->zoom_is_pressed = state;
        }
    }
}
