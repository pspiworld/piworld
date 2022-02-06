#include <math.h>
#include <stdio.h>
#include "chunks.h"
#include "clients.h"
#include "config.h"
#include "item.h"
#include "local_player.h"
#include "local_players.h"
#include "pg.h"
#include "pw.h"
#include "user_input.h"
#include "x11_event_handler.h"

LocalPlayer local_players[MAX_LOCAL_PLAYERS];
int auto_add_players_on_new_devices;

int limit_player_count_to_fit_gpu_mem(void);
void set_players_to_match_joysticks(void);

void local_players_init(void)
{
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = &local_players[i];
        local_player_init(local, &clients->players[i], i+1);
    }
    if (config->players == -1) {
        auto_add_players_on_new_devices = 1;
        set_players_to_match_joysticks();
    } else {
        auto_add_players_on_new_devices = 0;
        limit_player_count_to_fit_gpu_mem();
        set_player_count(clients, config->players);
    }
}

LocalPlayer *get_local_player(int p)
{
    return local_players + p;
}

// returns 1 if limit applied, 0 if no limit applied
int limit_player_count_to_fit_gpu_mem(void)
{
    if (!config->no_limiters &&
        pg_get_gpu_mem_size() < 64 && config->players > 2) {
        printf("More GPU memory needed for more players.\n");
        config->players = 2;
        return 1;
    }
    return 0;
}

void set_players_to_match_joysticks(void)
{
    int joystick_count = pg_joystick_count();
    if (joystick_count != config->players) {
        config->players = MAX(keyboard_player_count(), pg_joystick_count());
        config->players = MAX(1, config->players);
        config->players = MIN(MAX_LOCAL_PLAYERS, config->players);
        limit_player_count_to_fit_gpu_mem();
        set_player_count(clients, config->players);
    }
}

void change_player_count(int player_count)
{
    auto_add_players_on_new_devices = 0;
    config->players = player_count;
    limit_player_count_to_fit_gpu_mem();
    set_player_count(clients, config->players);
    recheck_view_radius();
}

void remove_player(LocalPlayer *local)
{
    client_remove_player(local->player->id);
    local->player->is_active = 0;
    auto_add_players_on_new_devices = 0;
    config->players -= 1;
    local->keyboard_id = UNASSIGNED;
    local->mouse_id = UNASSIGNED;
    local->joystick_id = UNASSIGNED;
    recheck_players_view_size();
    recheck_view_radius();
}

LocalPlayer *add_player_on_new_device(void)
{
    LocalPlayer *local = NULL;
    if (auto_add_players_on_new_devices &&
        config->players < MAX_LOCAL_PLAYERS) {
        config->players++;
        if (limit_player_count_to_fit_gpu_mem() == 0) {
            local = &local_players[config->players - 1];
            local->player->is_active = 1;
            recheck_view_radius();
        }
    }
    return local;
}

LocalPlayer* player_for_keyboard(int keyboard_id)
{
    // Match keyboard to assigned player
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = local_players + i;
        if (local->player->is_active && local->keyboard_id == keyboard_id) {
            return local;
        }
    }
    // Assign keyboard to next active player without one
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = local_players + i;
        if (local->player->is_active && local->keyboard_id == UNASSIGNED) {
            local->keyboard_id = keyboard_id;
            return local;
        }
    }
    // Add a new player and assign the keyboard to them
    LocalPlayer *local = add_player_on_new_device();
    if (local) {
        local->keyboard_id = keyboard_id;
        return local;
    }
    // If keyboard remains unassigned and all active players already have one
    // assign this keyboard to the first player.
    return local_players;
}

LocalPlayer* player_for_mouse(int mouse_id)
{
    // Match mouse to assigned player
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = local_players + i;
        if (local->player->is_active && local->mouse_id == mouse_id) {
            return local;
        }
    }
    // Assign mouse to next active player without a mouse and already assigned
    // a keyboard
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = local_players + i;
        if (local->player->is_active && local->mouse_id == UNASSIGNED &&
            local->keyboard_id != UNASSIGNED) {
            local->mouse_id = mouse_id;
            return local;
        }
    }
    // Assign mouse to next active player without one
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = local_players + i;
        if (local->player->is_active && local->mouse_id == UNASSIGNED) {
            local->mouse_id = mouse_id;
            return local;
        }
    }
    // If mouse remains unassigned and all active players already have one
    // assign this mouse to the first player.
    return local_players;
}

LocalPlayer* player_for_joystick(int joystick_id)
{
    // Match joystick to assigned player
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = local_players + i;
        if (local->player->is_active && local->joystick_id == joystick_id) {
            return local;
        }
    }
    // Assign joystick to next active player without one
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = local_players + i;
        if (local->player->is_active && local->joystick_id == UNASSIGNED) {
            local->joystick_id = joystick_id;
            return local;
        }
    }
    // Add a new player and assign the joystick to them
    LocalPlayer *local = add_player_on_new_device();
    if (local) {
        local->joystick_id = joystick_id;
        return local;
    }
    // If joystick remains unassigned and all active players already have one
    // assign this joystick to the first player.
    return local_players;
}

void set_players_view_size(int w, int h)
{
    int view_margin = 6;
    int active_count = 0;
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = &local_players[i];
        if (local->player->is_active) {
            active_count++;
            if (config->players == 1) {
                // Full size view
                local->view_x = 0;
                local->view_y = 0;
                local->view_width = w;
                local->view_height = h;
            } else if (config->players == 2) {
                // Half size views
                if (active_count == 1) {
                    local->view_x = 0;
                    local->view_y = 0;
                    local->view_width = w / 2 - (view_margin/2);
                    local->view_height = h;
                } else {
                    local->view_x = w / 2 + (view_margin/2);
                    local->view_y = 0;
                    local->view_width = w / 2 - (view_margin/2);
                    local->view_height = h;
                }
            } else {
                // Quarter size views
                if (local->player->id == 1) {
                    local->view_x = 0;
                    local->view_y = h / 2 + (view_margin/2);
                    local->view_width = w / 2 - (view_margin/2);
                    local->view_height = h / 2 - (view_margin/2);
                } else if (local->player->id == 2) {
                    local->view_x = w / 2 + (view_margin/2);
                    local->view_y = h / 2 + (view_margin/2);
                    local->view_width = w / 2 - (view_margin/2);
                    local->view_height = h / 2 - (view_margin/2);

                } else if (local->player->id == 3) {
                    local->view_x = 0;
                    local->view_y = 0;
                    local->view_width = w / 2 - (view_margin/2);
                    local->view_height = h / 2 - (view_margin/2);

                } else {
                    local->view_x = w / 2 + (view_margin/2);
                    local->view_y = 0;
                    local->view_width = w / 2 - (view_margin/2);
                    local->view_height = h / 2 - (view_margin/2);
                }
            }
        }
    }
}

void move_players_to_empty_block(void)
{
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = &local_players[i];
        State *s = &local->player->state;
        if (local->player->is_active) {
            int w = get_block(roundf(s->x), s->y, roundf(s->z));
            int shape = get_shape(roundf(s->x), s->y, roundf(s->z));
            int extra = get_extra(roundf(s->x), s->y, roundf(s->z));
            if (is_obstacle(w, shape, extra)) {
                s->y = highest_block(s->x, s->z) + 2;
            }
        }
    }
}

int keyboard_player_count(void)
{
    int count = 0;
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = local_players + i;
        if (local->player && local->player->is_active &&
            local->keyboard_id != UNASSIGNED) {
            count++;
        }
    }
    return count;
}

void move_local_player_keyboard_and_mouse_to_next_active_player(
    LocalPlayer *p)
{
    int keyboard_id = p->keyboard_id;
    int next = get_next_local_player(clients, p->player->id - 1);
    LocalPlayer *next_local = local_players + next;
    if (next_local != p) {
        next_local->keyboard_id = keyboard_id;
        p->keyboard_id = UNASSIGNED;
        if (p->mouse_id != UNASSIGNED) {
            next_local->mouse_id = p->mouse_id;
            p->mouse_id = UNASSIGNED;
        }
    }
}

void handle_mouse_motion(int mouse_id, float x, float y)
{
    LocalPlayer *local = player_for_mouse(mouse_id);
    local_player_handle_mouse_motion(local, x, y);
}

void handle_key_press(int keyboard_id, int mods, int keysym)
{
    int prev_player_count = config->players;
    LocalPlayer *local = player_for_keyboard(keyboard_id);
    if (prev_player_count != config->players) {
        // If a new keyboard resulted in a new player ignore this first event.
        return;
    }
    local_player_handle_key_press(local, mods, keysym);
}

void handle_key_release(int keyboard_id, int keysym)
{
    LocalPlayer *local = player_for_keyboard(keyboard_id);
    local_player_handle_key_release(local, keysym);
}

void handle_joystick_axis(PG_Joystick *j, int j_num, int axis, float value)
{
    int prev_player_count = config->players;
    LocalPlayer *local = player_for_joystick(j_num);
    if (prev_player_count != config->players) {
        // If a new gamepad resulted in a new player ignore this first event.
        return;
    }
    local_player_handle_joystick_axis(local, j, axis, value);
}

void handle_joystick_button(PG_Joystick *j, int j_num, int button, int state)
{
    int prev_player_count = config->players;
    LocalPlayer *local = player_for_joystick(j_num);
    if (prev_player_count != config->players) {
        // If a new gamepad resulted in a new player ignore this first event.
        return;
    }
    local_player_handle_joystick_button(local, j, button, state);
}

void handle_mouse_press(int mouse_id, int b)
{
    if (!relative_mouse_in_use()) {
        return;
    }
    LocalPlayer *local = player_for_mouse(mouse_id);
    int mods = 0;
    if (local->keyboard_id != UNASSIGNED) {
        mods = pg_get_mods(local->keyboard_id);
    }
    local_player_handle_mouse_press(local, b, mods);
}


void handle_mouse_release(int mouse_id, int b)
{
    if (!relative_mouse_in_use()) {
        return;
    }
    LocalPlayer *local = player_for_mouse(mouse_id);
    int mods = 0;
    if (local->keyboard_id != UNASSIGNED) {
        mods = pg_get_mods(local->keyboard_id);
    }
    local_player_handle_mouse_release(local, b, mods);
}

void handle_window_close(void)
{
    pw_exit();
}

void handle_focus_out(void) {
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *p = &local_players[i];
        cancel_player_inputs(p);
    }

    pg_fullscreen(0);
}

