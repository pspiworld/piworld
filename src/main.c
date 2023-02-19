
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chunks.h"
#include "clients.h"
#include "db.h"
#include "fence.h"
#include "local_players.h"
#include "pg.h"
#include "pw.h"
#include "pwlua_startup.h"
#include "pwlua_standalone.h"
#include "render.h"
#include "user_input.h"
#include "x11_event_handler.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

static int terminate;

int main(int argc, char **argv)
{
    int override_worldgen_from_command_line = 0;
    // INITIALIZATION //
    init_data_dir();
    srand(time(NULL));
    rand();
    reset_config();
    parse_startup_config(argc, argv);
    if (strlen(config->worldgen_path) > 0) {
        set_worldgen(config->worldgen_path);
        override_worldgen_from_command_line = 1;
    }

    if (config->benchmark_create_chunks) {
        if (config->benchmark_create_chunks < MAX_CHUNKS &&
            config->benchmark_create_chunks > 0) {
            benchmark_chunks(config->benchmark_create_chunks);
        } else {
            printf("Invalid chunk count: %d\n",
                   config->benchmark_create_chunks);
        }
        return EXIT_SUCCESS;
    }

    if (config->lua_standalone) {
        pwlua_standalone_REPL();
        return EXIT_SUCCESS;  //TODO: exit status of lua instance
    }

    pw_init();

    // SHAPE INITIALIZATION //
    fence_init();

    pwlua_startup_init();

    // WINDOW INITIALIZATION //
    pg_start(config->window_title, config->window_x, config->window_y,
             config->window_width, config->window_height);
    pg_swap_interval(config->vsync);
    set_key_press_handler(*handle_key_press);
    set_key_release_handler(*handle_key_release);
    set_mouse_press_handler(*handle_mouse_press);
    set_mouse_release_handler(*handle_mouse_release);
    set_mouse_motion_handler(*handle_mouse_motion);
    set_window_close_handler(*handle_window_close);
    set_focus_out_handler(*handle_focus_out);
    if (config->verbose) {
        pg_print_info();
    }
    pg_init_joysticks();
    pg_set_fullscreen_size(config->fullscreen_width, config->fullscreen_height);
    if (config->fullscreen) {
        pg_fullscreen(1);
    }
    set_view_radius(config->view, config->delete_radius);

    if (config->ignore_gamepad == 0) {
        pg_set_joystick_button_handler(*handle_joystick_button);
        pg_set_joystick_axis_handler(*handle_joystick_axis);
    }

    user_input_init();
    render_init();

    // OUTER LOOP //
    int running = 1;
    while (running) {

        // INITIALIZE WORKER THREADS
        initialize_worker_threads();

        // DATABASE INITIALIZATION //
        if (is_offline() || config->use_cache) {
            db_enable();
            if (db_init(get_db_path())) {
                return EXIT_FAILURE;
            }
            if (is_online()) {
                // TODO: support proper caching of signs (handle deletions)
                db_delete_all_signs();
            } else {
                // Setup worldgen from local config
                const unsigned char *value;
                if (!override_worldgen_from_command_line) {
                    value = db_get_option("worldgen");
                    if (value != NULL) {
                        snprintf(config->worldgen_path, MAX_PATH_LENGTH, "%s", value);
                        set_worldgen(config->worldgen_path);
                    } else {
                        set_worldgen(NULL);
                    }
                } else if (strlen(config->worldgen_path) == 0) {
                    set_worldgen(NULL);
                }
                override_worldgen_from_command_line = 0;
                value = db_get_option("show-clouds");
                if (value != NULL) {
                    config->show_clouds = atoi((char *)value);
                }
                value = db_get_option("show-plants");
                if (value != NULL) {
                    config->show_plants = atoi((char *)value);
                }
                value = db_get_option("show-trees");
                if (value != NULL) {
                    config->show_trees = atoi((char *)value);
                }
            }
        }

        // CLIENT INITIALIZATION //
        if (is_online()) {
            client_enable();
            client_connect(config->server, config->port);
            client_start();
            client_version(2);
            login();
        }

        // LOCAL VARIABLES //
        reset_model();
        FPS fps = {0, 0, 0};
        double last_commit = pg_get_time();
        double last_update = pg_get_time();

        int check_players_position = 1;

        for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
            Player *player = local_players[i].player;
            if (player->is_active) {
                client_add_player(player->id);
            }
        }

        // BEGIN MAIN LOOP //
        double previous = pg_get_time();
        while (!terminate) {
            // WINDOW SIZE AND SCALE //
            pw_setup_window();

            // FRAME RATE //
            if (check_time_changed()) {
                last_commit = pg_get_time();
                last_update = pg_get_time();
                memset(&fps, 0, sizeof(fps));
            }
            update_fps(&fps);
            double now = pg_get_time();
            double dt = now - previous;
            dt = MIN(dt, 0.2);
            dt = MAX(dt, 0.0);
            previous = now;

            // DRAIN EDIT QUEUE //
            drain_edit_queue(100000, 0.005, now);

            pwlua_remove_closed_threads();

            // HANDLE MOVEMENT //
            for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
                LocalPlayer *local = &local_players[i];
                if (local->player->is_active) {
                    handle_movement(dt, local);
                }
            }

            // HANDLE JOYSTICK INPUT //
            pg_poll_joystick_events();

            // HANDLE DATA FROM SERVER //
            char *buffer = client_recv();
            if (buffer) {
                parse_buffer(buffer);
                free(buffer);
            }

            // FLUSH DATABASE //
            if (now - last_commit > COMMIT_INTERVAL) {
                last_commit = now;
                db_commit();
            }

            // SEND POSITION TO SERVER //
            if (now - last_update > 0.1) {
                last_update = now;
                for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
                    LocalPlayer *local = &local_players[i];
                    if (local->player->is_active) {
                        State *s = &local->player->state;
                        client_position(local->player->id,
                                        s->x, s->y, s->z, s->rx, s->ry);
                    }
                }
            }

            // PREPARE TO RENDER //
            delete_chunks(get_delete_radius());
            for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
                Player *player = local_players[i].player;
                if (player->is_active) {
                    State *s = &player->state;
                    del_buffer(player->buffer);
                    player->buffer = gen_player_buffer(s->x, s->y, s->z, s->rx,
                        s->ry, player->texture_index);
                }
            }
            for (int i = 1; i < client_count; i++) {
                Client *client = clients + i;
                for (int j = 0; j < MAX_LOCAL_PLAYERS; j++) {
                    Player *remote_player = client->players + j;
                    if (remote_player->is_active) {
                        interpolate_player(remote_player);
                    }
                }
            }

            // RENDER //
            glClear(GL_COLOR_BUFFER_BIT);
            glClear(GL_DEPTH_BUFFER_BIT);
            for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
                LocalPlayer *local = &local_players[i];
                if (local->player->is_active) {
                    render_player_world(local, fps);
                }
            }

#ifdef DEBUG
            check_gl_error();
#endif

            // SWAP AND POLL //
            pg_swap_buffers();
            pg_next_event();
            if (check_mode_changed()) {
                break;
            }
            if (check_players_position) {
                move_players_to_empty_block();
                check_players_position = 0;
            }
            if (check_render_option_changed()) {
                check_players_position = 1;
                deinitialize_worker_threads();
                initialize_worker_threads();
                delete_all_chunks();
            }
        }
        if (terminate) {
            running = 0;
        }

        // DEINITIALIZE WORKER THREADS
        deinitialize_worker_threads();

        // SHUTDOWN //
        pw_unload_game();
    }
    render_deinit();
    user_input_deinit();
    pw_deinit();

    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = &local_players[i];
        local_player_clear(local);
    }

    pwlua_startup_deinit();

    pg_terminate_joysticks();
    pg_end();
    return EXIT_SUCCESS;
}

void pw_exit(void)
{
    terminate = 1;
}

