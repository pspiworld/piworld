#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "action.h"
#include "chunks.h"
#include "local_players.h"
#include "pw.h"
#include "stb_ds.h"
#include "user_input.h"
#include "util.h"
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

void action_apply_bindings(LocalPlayer *local, char *bindings_str)
{
    char *key1, *key2;
    char *binding = tokenize(bindings_str, ",", &key1);
    action_t action = NULL;
    while (binding) {
        char *token = tokenize(binding, ":", &key2);

        if (strlen(token) > 1 && token[0] == 'b') {
            int button = atoi(token + 1);
            action = get_action_from_name(key2);
            if (button >= 0 && button < MAX_GAMEPAD_BUTTONS && action != NULL) {
                local->gamepad_bindings[button] = action;
            }
        } else if (strlen(token) > 1 && token[0] == 'a') {
            int axis = atoi(token + 1);
            action = get_action_from_name(key2);
            if (axis >= 0 && axis < MAX_GAMEPAD_AXES && action != NULL) {
                local->gamepad_axes_bindings[axis] = action;
            }
        } else if (strlen(token) == 3 && token[1] == 'm' && token[2] == 'b') {
            int button = 0;
            action = get_action_from_name(key2);
            if (isdigit(token[0])) {
                button = atoi(token);
            } else if (token[0] == 'l') {
                button = 1;
            } else if (token[0] == 'm') {
                button = 2;
            } else if (token[0] == 'r') {
                button = 3;
            }

            if (button > 0 && button < MAX_MOUSE_BUTTONS && action != NULL) {
                local->mouse_bindings[button] = action;
            }
        } else {
            // Assume key binding
            int keysym = XStringToKeysym(token);
            action = get_action_from_name(key2);
            hmput(local->key_bindings, keysym, action);
        }

        binding = tokenize(NULL, ",", &key1);
    }
}

action_t get_action_from_name(char *name)
{
    static const char *action_names[] = {
        "add_block",
        "crouch",
        "fly_mode_toggle",
        "jump",
        "menu",
        "mode1",
        "mode2",
        "next_item_in_hand",
        "next_mode",
        "ortho",
        "pick_item_in_hand",
        "previous_item_in_hand",
        "remove_block",
        "view_down",
        "view_left",
        "view_right",
        "view_up",
        "zoom",
        "move_left_right",
        "move_forward_back",
        "view_left_right",
        "view_up_down",
        "move_right_left",
        "move_back_forward",
        "view_right_left",
        "view_down_up",
        "move_forward",
        "move_back",
        "move_left",
        "move_right",
        "open_item_in_hand_menu",
        "undo",
        "set_mouse_absolute",
        "set_mouse_relative",
        "move_local_player_keyboard_and_mouse_to_next_active_player",
        "fullscreen_toggle",
        "observe_view_toggle",
        "picture_in_picture_observe_view_toggle",
        "open_chat_command_line",
        "open_action_command_line",
        "open_lua_command_line",
        "open_sign_command_line",
        "primary",
        "secondary",
        "primary_mouse_action",
        "secondary_mouse_action",
        "tertiary_mouse_action",
        "open_menu_on_release",
        "show_input_event",
        "crouch_toggle",
        "ortho_toggle",
        "zoom_toggle",
        "vt",
    };
    static const int action_count = sizeof(action_names) / sizeof(char *);
    static const action_t actions[] = {
        &action_add_block,
        &action_crouch,
        &action_fly_mode_toggle,
        &action_jump,
        &action_menu,
        &action_mode1,
        &action_mode2,
        &action_next_item_in_hand,
        &action_next_mode,
        &action_ortho,
        &action_pick_item_in_hand,
        &action_previous_item_in_hand,
        &action_remove_block,
        &action_view_down,
        &action_view_left,
        &action_view_right,
        &action_view_up,
        &action_zoom,
        &action_move_left_right,
        &action_move_forward_back,
        &action_view_left_right,
        &action_view_up_down,
        &action_move_right_left,
        &action_move_back_forward,
        &action_view_right_left,
        &action_view_down_up,
        &action_move_forward,
        &action_move_back,
        &action_move_left,
        &action_move_right,
        &action_open_item_in_hand_menu,
        &action_undo,
        &action_set_mouse_absolute,
        &action_set_mouse_relative,
        &action_move_local_player_keyboard_and_mouse_to_next_active_player,
        &action_fullscreen_toggle,
        &action_observe_view_toggle,
        &action_picture_in_picture_observe_view_toggle,
        &action_open_chat_command_line,
        &action_open_action_command_line,
        &action_open_lua_command_line,
        &action_open_sign_command_line,
        &action_primary,
        &action_secondary,
        &action_primary_mouse_action,
        &action_secondary_mouse_action,
        &action_tertiary_mouse_action,
        &action_open_menu_on_release,
        &action_show_input_event,
        &action_crouch_toggle,
        &action_ortho_toggle,
        &action_zoom_toggle,
        &action_vt,
    };

    for (int i=0; i<action_count; i++) {
        if (strcmp(name, action_names[i]) == 0) {
            return actions[i];
        }
    }

    return NULL;
}

void action_view_up(LocalPlayer *local, Event *e)
{
    local->view_up_is_pressed = e->state;
    local->view_speed_up_down = 1;
}

void action_view_down(LocalPlayer *local, Event *e)
{
    local->view_down_is_pressed = e->state;
    local->view_speed_up_down = 1;
}

void action_view_left(LocalPlayer *local, Event *e)
{
    local->view_left_is_pressed = e->state;
    local->view_speed_left_right = 1;
}

void action_view_right(LocalPlayer *local, Event *e)
{
    local->view_right_is_pressed = e->state;
    local->view_speed_left_right = 1;
}

void action_move_forward(LocalPlayer *local, Event *e)
{
    local->forward_is_pressed = e->state;
    local->movement_speed_forward_back = 1;
}

void action_move_back(LocalPlayer *local, Event *e)
{
    local->back_is_pressed = e->state;
    local->movement_speed_forward_back = 1;
}

void action_move_left(LocalPlayer *local, Event *e)
{
    local->left_is_pressed = e->state;
    local->movement_speed_left_right = 1;
}

void action_move_right(LocalPlayer *local, Event *e)
{
    local->right_is_pressed = e->state;
    local->movement_speed_left_right = 1;
}

void action_show_input_event(LocalPlayer *local, Event *e)
{
    char msg[128];
    snprintf(msg, 128, "type: %d button: %d state: %d x: %f y: %f mods: %d",
        e->type, e->button, e->state, e->x, e->y, e->mods);
    add_message(local->player->id, msg);
}

void action_add_block(LocalPlayer *local, Event *e)
{
    if (e->state) {
        set_block_under_crosshair(local);
    }
}

void action_remove_block(LocalPlayer *local, Event *e)
{
    if (e->state) {
        clear_block_under_crosshair(local);
    }
}

void action_jump(LocalPlayer *local, Event *e)
{
    local->jump_is_pressed = e->state;
}

void action_crouch(LocalPlayer *local, Event *e)
{
    local->crouch_is_pressed = e->state;
}

void action_next_item_in_hand(LocalPlayer *local, Event *e)
{
    if (e->state) {
        cycle_item_in_hand_up(local);
    }
}

void action_previous_item_in_hand(LocalPlayer *local, Event *e)
{
    if (e->state) {
        cycle_item_in_hand_down(local);
    }
}

void action_zoom(LocalPlayer *local, Event *e)
{
    local->zoom_is_pressed = e->state;
}

void action_ortho(LocalPlayer *local, Event *e)
{
    local->ortho_is_pressed = e->state;
}

void action_pick_item_in_hand(LocalPlayer *local, Event *e)
{
    if (e->state) {
        set_item_in_hand_to_item_under_crosshair(local);
    }
}

void action_fly_mode_toggle(LocalPlayer *local, Event *e)
{
    if (e->state) {
        local->flying = !local->flying;
    }
}

void action_mode1(LocalPlayer *local, Event *e)
{
    action_t action = NULL;
    switch (local->shoulder_button_mode) {
        case CROUCH_JUMP:
            action = &action_jump;
            break;
        case REMOVE_ADD:
            action = &action_add_block;
            break;
        case CYCLE_DOWN_UP:
            action = &action_next_item_in_hand;
            break;
        case FLY_PICK:
            action = &action_pick_item_in_hand;
            break;
        case ZOOM_ORTHO:
            action = &action_ortho;
            break;
    }
    if (action != NULL) {
        action(local, e);
        return;
    }
}

void action_mode2(LocalPlayer *local, Event *e)
{
    action_t action = NULL;
    switch (local->shoulder_button_mode) {
        case CROUCH_JUMP:
            action = &action_crouch;
            break;
        case REMOVE_ADD:
            action = &action_remove_block;
            break;
        case CYCLE_DOWN_UP:
            action = &action_previous_item_in_hand;
            break;
        case FLY_PICK:
            action = &action_fly_mode_toggle;
            break;
        case ZOOM_ORTHO:
            action = &action_zoom;
            break;
    }
    if (action != NULL) {
        action(local, e);
        return;
    }
}

void action_next_mode(LocalPlayer *local, Event *e)
{
    if (e->state) {
        local->shoulder_button_mode += 1;
        local->shoulder_button_mode %= SHOULDER_BUTTON_MODE_COUNT;
        add_message(local->player->id,
                    shoulder_button_modes[local->shoulder_button_mode]);
    }
}

void action_menu(LocalPlayer *local, Event *e)
{
    if (e->state) {
        if (local->active_menu) {
            close_menu(local);
        } else {
            open_menu(local, &local->menu);
        }
    }
}

void action_open_item_in_hand_menu(LocalPlayer *local, Event *e)
{
    if (e->state) {
        open_menu(local, &local->menu_item_in_hand);
    }
}

void action_move_left_right(LocalPlayer *local, Event *e)
{
    local->left_is_pressed = (e->x < 0) ? 1 : 0;
    local->right_is_pressed = (e->x > 0) ? 1 : 0;
    local->movement_speed_left_right = fabs(e->x);
}

void action_move_forward_back(LocalPlayer *local, Event *e)
{
    local->forward_is_pressed = (e->x < 0) ? 1 : 0;
    local->back_is_pressed = (e->x > 0) ? 1 : 0;
    local->movement_speed_forward_back = fabs(e->x);
}

void action_view_up_down(LocalPlayer *local, Event *e)
{
    local->view_speed_up_down = fabs(e->x) * 2.0;
    local->view_up_is_pressed = (e->x < -DEADZONE);
    local->view_down_is_pressed = (e->x > DEADZONE);
}

void action_view_left_right(LocalPlayer *local, Event *e)
{
    local->view_speed_left_right = fabs(e->x) * 2.0;
    local->view_left_is_pressed = (e->x < -DEADZONE);
    local->view_right_is_pressed = (e->x > DEADZONE);
}

void action_move_right_left(LocalPlayer *local, Event *e)
{
    local->right_is_pressed = (e->x < 0) ? 1 : 0;
    local->left_is_pressed = (e->x > 0) ? 1 : 0;
    local->movement_speed_left_right = fabs(e->x);
}

void action_move_back_forward(LocalPlayer *local, Event *e)
{
    local->back_is_pressed = (e->x < 0) ? 1 : 0;
    local->forward_is_pressed = (e->x > 0) ? 1 : 0;
    local->movement_speed_forward_back = fabs(e->x);
}

void action_view_right_left(LocalPlayer *local, Event *e)
{
    local->view_speed_left_right = fabs(e->x) * 2.0;
    local->view_right_is_pressed = (e->x < -DEADZONE);
    local->view_left_is_pressed = (e->x > DEADZONE);
}

void action_view_down_up(LocalPlayer *local, Event *e)
{
    local->view_speed_up_down = fabs(e->x) * 2.0;
    local->view_down_is_pressed = (e->x < -DEADZONE);
    local->view_up_is_pressed = (e->x > DEADZONE);
}

void action_undo(LocalPlayer *local, Event *e)
{
    if (e->state == 1) {
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
    }
}

void action_set_mouse_absolute(LocalPlayer *local, Event *e)
{
    if (e->state == 1) {
        set_mouse_absolute();
    }
}

void action_set_mouse_relative(LocalPlayer *local, Event *e)
{
    if (e->state == 1) {
        set_mouse_relative();
    }
}

void action_move_local_player_keyboard_and_mouse_to_next_active_player(LocalPlayer *local, Event *e)
{
    if (e->state == 1) {
        move_local_player_keyboard_and_mouse_to_next_active_player(local);
    }
}

void action_fullscreen_toggle(LocalPlayer *local, Event *e)
{
    if (e->state == 1) {
        pg_toggle_fullscreen();
    }
}

void action_observe_view_toggle(LocalPlayer *local, Event *e)
{
    if (e->state == 1) {
        toggle_observe_view(local);
    }
}

void action_picture_in_picture_observe_view_toggle(LocalPlayer *local, Event *e)
{
    if (e->state == 1) {
        toggle_picture_in_picture_observe_view(local);
    }
}

void action_open_action_command_line(LocalPlayer *local, Event *e)
{
    if (e->state == 1) {
        open_action_command_line(local);
    }
}

void action_open_chat_command_line(LocalPlayer *local, Event *e)
{
    if (e->state == 1) {
        open_chat_command_line(local);
    }
}

void action_open_lua_command_line(LocalPlayer *local, Event *e)
{
    if (e->state == 1) {
        open_lua_command_line(local);
    }
}

void action_open_sign_command_line(LocalPlayer *local, Event *e)
{
    if (e->state == 1) {
        open_sign_command_line(local);
    }
}

void action_set_item_index_from_keysym_number(LocalPlayer *local, Event *e)
{
    if (e->state == 1) {
        if (e->button == XK_0) {
            local->item_index = 9;
        } else {
            // XK_1 to XK_9
            local->item_index = e->button - XK_1;
        }
    }
}

void action_primary(LocalPlayer *local, Event *e)
{
    if (e->state == 1) {
        if (e->mods & ControlMask) {
            set_block_under_crosshair(local);
        } else {
            clear_block_under_crosshair(local);
        }
    }
}

void action_secondary(LocalPlayer *local, Event *e)
{
    if (e->state == 1) {
        if (e->mods & ControlMask) {
            on_light(local);
        } else {
            set_block_under_crosshair(local);
        }
    }
}

void action_primary_mouse_action(LocalPlayer *local, Event *e)
{
    if (e->state == 0) {  // activate on mouse button release
        if (e->mods & ControlMask) {
            set_block_under_crosshair(local);
        } else {
            clear_block_under_crosshair(local);
        }
    }
}

void action_secondary_mouse_action(LocalPlayer *local, Event *e)
{
    if (e->state == 0) {  // activate on mouse button release
        if (e->mods & ControlMask) {
            on_light(local);
        } else {
            set_block_under_crosshair(local);
        }
    }
}

void action_tertiary_mouse_action(LocalPlayer *local, Event *e)
{
    if (e->state == 0) {  // activate on mouse button release
        open_menu_for_item_under_crosshair(local);
    }
}

void action_open_menu_on_release(LocalPlayer *local, Event *e)
{
    if (e->state == 0) {  // activate on mouse button release
        open_menu(local, &local->menu);
    }
}

void action_crouch_toggle(LocalPlayer *local, Event *e)
{
    if (e->state) {
        local->crouch_is_pressed = !local->crouch_is_pressed;
    }
}

void action_ortho_toggle(LocalPlayer *local, Event *e)
{
    if (e->state) {
        local->ortho_is_pressed = !local->ortho_is_pressed;
    }
}

void action_zoom_toggle(LocalPlayer *local, Event *e)
{
    if (e->state) {
        local->zoom_is_pressed = !local->zoom_is_pressed;
    }
}

void action_vt(LocalPlayer *local, Event *e)
{
    if (e->state) {
        if (!local->vt_open) {
            open_vt(local);
        } else {
            close_vt(local);
        }
    }
}
