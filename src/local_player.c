#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "chunk.h"
#include "chunks.h"
#include "client.h"
#include "config.h"
#include "db.h"
#include "fence.h"
#include "item.h"
#include "local_player.h"
#include "pg.h"
#include "pw.h"
#include "x11_event_handler.h"

void create_menus(LocalPlayer *local);
void populate_load_menu(LocalPlayer *local);
void populate_script_run_menu(LocalPlayer *local);
void populate_shape_menu(Menu *menu);
void populate_texture_menu(Menu *menu);
void populate_transform_menu(Menu *menu);
void populate_worldgen_select_menu(LocalPlayer *local);

void local_player_init(LocalPlayer *local, Player *player, int player_id)
{
    reset_history(local);
    create_menus(local);
    local->player = player;
    local->player->id = player_id;
}

void local_player_reset(LocalPlayer *local)
{
    int i = local->player->id - 1;
    local->flying = 0;
    local->item_index = 0;
    local->typing = 0;
    local->observe1 = 0;
    local->observe1_client_id = 0;
    local->observe2 = 0;
    local->observe2_client_id = 0;
    memset(local->typing_buffer, 0, sizeof(char) * MAX_TEXT_LENGTH);
    memset(local->messages, 0,
           sizeof(char) * MAX_MESSAGES * MAX_TEXT_LENGTH);
    local->message_index = 0;
    local->has_undo_block = 0;
    local->undo_block.has_sign = 0;

    local->player->name[0] = '\0';
    local->player->buffer = 0;
    local->player->texture_index = i;

    local->mouse_id = UNASSIGNED;
    local->keyboard_id = UNASSIGNED;
    local->joystick_id = UNASSIGNED;

    if (local->lua_shell == NULL) {
        local->lua_shell = pwlua_new_shell(local->player->id);
    }

    // LOAD STATE FROM DATABASE //
    State *s = &local->player->state;
    int loaded = db_load_state(&s->x, &s->y, &s->z, &s->rx, &s->ry, i);
    force_chunks(local->player, get_float_size());

    loaded = db_load_player_name(local->player->name, MAX_NAME_LENGTH, i);
    if (!loaded) {
        snprintf(local->player->name, MAX_NAME_LENGTH, "player%d",
                 local->player->id);
    }

    history_load(local);
}

void local_player_clear_menus(LocalPlayer *local)
{
    menu_clear_items(&local->menu);
    menu_clear_items(&local->menu_options);
    menu_clear_items(&local->menu_new);
    menu_clear_items(&local->menu_load);
    menu_clear_items(&local->menu_item_in_hand);
    menu_clear_items(&local->menu_block_edit);
    menu_clear_items(&local->menu_texture);
    menu_clear_items(&local->menu_shape);
    menu_clear_items(&local->menu_script);
    menu_clear_items(&local->menu_script_run);
    menu_clear_items(&local->menu_worldgen);
    menu_clear_items(&local->menu_worldgen_select);
}

void local_player_clear(LocalPlayer *local)
{
    local_player_clear_menus(local);
    if (local->lua_shell != NULL) {
        pwlua_remove(local->lua_shell);
    }
}

void open_menu(LocalPlayer *local, Menu *menu)
{
    local->active_menu = menu;
    menu_handle_mouse(local->active_menu, local->mouse_x, local->mouse_y);
    cancel_player_inputs(local);
}

void close_menu(LocalPlayer *local)
{
    local->active_menu = NULL;
}

void handle_menu_event(LocalPlayer *local, Menu *menu, int event)
{
    if (event == MENU_CANCELLED) {
        close_menu(local);
    } else if (menu == &local->menu) {
        if (event == local->menu_id_resume) {
            close_menu(local);
        } else if (event == local->menu_id_options) {
            open_menu(local, &local->menu_options);
        } else if (event == local->menu_id_new) {
            open_menu(local, &local->menu_new);
            menu_set_highlighted_item(&local->menu_new,
                local->menu_id_new_game_name);
        } else if (event == local->menu_id_load) {
            populate_load_menu(local);
            open_menu(local, &local->menu_load);
        } else if (event == local->menu_id_exit) {
            pw_exit();
        }
    } else if (menu == &local->menu_options) {
        if (event == local->menu_id_options_resume) {
            close_menu(local);
        } else if (event == local->menu_id_script) {
            open_menu(local, &local->menu_script);
        } else if (event == local->menu_id_worldgen) {
            open_menu(local, &local->menu_worldgen);
        } else if (event == local->menu_id_fullscreen) {
            pg_toggle_fullscreen();
        } else if (event == local->menu_id_crosshairs) {
            config->show_crosshairs = menu_get_option(&local->menu_options,
                local->menu_id_crosshairs);
        } else if (event == local->menu_id_verbose) {
            config->verbose = menu_get_option(&local->menu_options,
                local->menu_id_verbose);
        } else if (event == local->menu_id_wireframe) {
            config->show_wireframe = menu_get_option(&local->menu_options,
                local->menu_id_wireframe);
        }
    } else if (menu == &local->menu_new) {
        if (event == local->menu_id_new_cancel) {
            close_menu(local);
        } else if (event == local->menu_id_new_ok ||
                   event == local->menu_id_new_game_name) {
            close_menu(local);
            char *new_game_name = menu_get_line_edit(menu,
                local->menu_id_new_game_name);
            char new_game_path[MAX_PATH_LENGTH];
            if (strlen(new_game_name) == 0) {
                add_message(local->player->id, "New game needs a name");
                return;
            }
            snprintf(new_game_path, MAX_PATH_LENGTH, "%s/%s.piworld",
                config->path, new_game_name);
            if (access(new_game_path, F_OK) != -1) {
                add_message(local->player->id,
                    "A game with that name already exists");
                return;
            }
            pw_new_game(new_game_path);
        }
    } else if (menu == &local->menu_load) {
        if (event == local->menu_id_load_cancel) {
            close_menu(local);
        } else if (event > 0) {
            // Load an existing game
            char game_path[MAX_PATH_LENGTH];
            snprintf(game_path, MAX_PATH_LENGTH, "%s/%s.piworld", config->path,
                     menu_get_name(menu, event));
            if (access(game_path, F_OK) == -1) {
                add_message(local->player->id, "Game file not found");
                return;
            }
            pw_load_game(game_path);
            close_menu(local);
        }
    } else if (menu == &local->menu_item_in_hand) {
        if (event == local->menu_id_item_in_hand_cancel) {
            close_menu(local);
        } else if (event > 0) {
            local->item_index = event - 1;
            close_menu(local);
        }
    } else if (menu == &local->menu_block_edit) {
        if (event == local->menu_id_block_edit_resume) {
            close_menu(local);
        } else if (event == local->menu_id_texture) {
            open_menu(local, &local->menu_texture);
        } else if (event == local->menu_id_shape) {
            open_menu(local, &local->menu_shape);
        } else if (event == local->menu_id_sign_text) {
            set_sign(local->edit_x, local->edit_y, local->edit_z,
                     local->edit_face, menu_get_line_edit(menu, event));
        } else if (event == local->menu_id_transform) {
            int w = atoi(menu_get_line_edit(menu, event));
            set_transform(local->edit_x, local->edit_y, local->edit_z, w);
        } else if (event == local->menu_id_light) {
            int w = atoi(menu_get_line_edit(menu, event));
            set_light(chunked(local->edit_x), chunked(local->edit_z),
                      local->edit_x, local->edit_y, local->edit_z, w);
        } else if (event == local->menu_id_control_bit) {
            int cb = menu_get_option(menu, local->menu_id_control_bit);
            int w = get_extra(local->edit_x, local->edit_y, local->edit_z);
            if (cb) {
                w |= EXTRA_BIT_CONTROL;
            } else {
                w &= ~EXTRA_BIT_CONTROL;
            }
            set_extra(local->edit_x, local->edit_y, local->edit_z, w);
        } else if (event == local->menu_id_open_bit) {
            int open = menu_get_option(menu, local->menu_id_open_bit);
            int w = get_extra(local->edit_x, local->edit_y, local->edit_z);
            if (open) {
                w |= EXTRA_BIT_OPEN;
            } else {
                w &= ~EXTRA_BIT_OPEN;
            }
            set_extra(local->edit_x, local->edit_y, local->edit_z, w);
        }
    } else if (menu == &local->menu_texture) {
        if (event == local->menu_id_texture_cancel) {
            close_menu(local);
        } else if (event > 0) {
            set_block(local->edit_x, local->edit_y, local->edit_z,
                      items[event - 1]);
            close_menu(local);
        }
    } else if (menu == &local->menu_shape) {
        if (event == local->menu_id_shape_cancel) {
            close_menu(local);
        } else if (event > 0) {
            set_shape(local->edit_x, local->edit_y, local->edit_z,
                      shapes[event - 1]);
            close_menu(local);
        }
    } else if (menu == &local->menu_script) {
        if (event == local->menu_id_script_cancel) {
            close_menu(local);
        } else if (event == local->menu_id_script_run) {
            populate_script_run_menu(local);
            open_menu(local, &local->menu_script_run);
        }
    } else if (menu == &local->menu_worldgen) {
        if (event == local->menu_id_worldgen_cancel) {
            close_menu(local);
        } else if (event == local->menu_id_worldgen_select) {
            populate_worldgen_select_menu(local);
            open_menu(local, &local->menu_worldgen_select);
        }
    } else if (menu == &local->menu_script_run) {
        if (event == local->menu_id_script_run_cancel) {
            close_menu(local);
        } else if (event > 0) {
            char path[MAX_PATH_LENGTH];
            struct stat sb;
            snprintf(path, MAX_PATH_LENGTH, "%s/%s",
                     local->menu_script_run_dir, menu_get_name(menu, event));
            char *tmp = realpath(path, NULL);
            snprintf(path, MAX_PATH_LENGTH, tmp);
            free(tmp);
            stat(path, &sb);
            if (S_ISDIR(sb.st_mode)) {
                snprintf(local->menu_script_run_dir, MAX_DIR_LENGTH, path);
                populate_script_run_menu(local);
                open_menu(local, &local->menu_script_run);
                return;
            }
            char lua_code[LUA_MAXINPUT];
            snprintf(lua_code, sizeof(lua_code), "$dofile(\"%s\")\n", path);
            LuaThreadState* new_lua_instance = pwlua_new(local->player->id);
            pwlua_parse_line(new_lua_instance, lua_code);
        }
    } else if (menu == &local->menu_worldgen_select) {
        if (event == local->menu_id_worldgen_select_cancel) {
            close_menu(local);
        } else if (event > 0) {
            set_worldgen(menu_get_name(menu, event));
            db_set_option("worldgen", menu_get_name(menu, event));
        }
    }
}

void cancel_player_inputs(LocalPlayer *p)
{
    p->forward_is_pressed = 0;
    p->back_is_pressed = 0;
    p->left_is_pressed = 0;
    p->right_is_pressed = 0;
    p->jump_is_pressed = 0;
    p->crouch_is_pressed = 0;
    p->view_left_is_pressed = 0;
    p->view_right_is_pressed = 0;
    p->view_up_is_pressed = 0;
    p->view_down_is_pressed = 0;
    p->ortho_is_pressed = 0;
    p->zoom_is_pressed = 0;
}

void reset_history(LocalPlayer *local)
{
    local->history_position = NOT_IN_HISTORY;
    for (int i=0; i<NUM_HISTORIES; i++) {
        local->typing_history[i].end = -1;
        local->typing_history[i].size = 0;
    }
    local->typing_history[CHAT_HISTORY].line_start = 0;
    local->typing_history[COMMAND_HISTORY].line_start = 1;
    local->typing_history[SIGN_HISTORY].line_start = 1;
    local->typing_history[LUA_HISTORY].line_start = 1;
}

void history_add(TextLineHistory *history, char *line)
{
    if (MAX_HISTORY_SIZE == 0 || strlen(line) <= history->line_start) {
        // Ignore empty lines
        return;
    }

    int duplicate_line = 1;
    if (history->size == 0 ||
        strcmp(line, history->lines[history->end]) != 0) {
        duplicate_line = 0;
    }
    if (!duplicate_line) {
        // Add a non-duplicate line to the history
        if (history->size >= MAX_HISTORY_SIZE) {
            memmove(history->lines, history->lines + 1,
                    (MAX_HISTORY_SIZE-1) * MAX_TEXT_LENGTH);
        }
        history->size = MIN(history->size + 1, MAX_HISTORY_SIZE);
        snprintf(history->lines[history->size - 1], MAX_TEXT_LENGTH, "%s",
                 line);
        history->end = history->size - 1;
    }
}

void history_previous(TextLineHistory *history, char *line, int *position)
{
    if (history->size == 0) {
        return;
    }

    int new_position;
    if (*position == NOT_IN_HISTORY) {
        new_position = history->end;
    } else if (*position == 0) {
        new_position = history->size - 1;
    } else {
        new_position = (*position - 1) % history->size;
    }

    if (*position != NOT_IN_HISTORY && new_position == history->end) {
        // Stop if already at start of history
        return;
    }

    *position = new_position;
    snprintf(line, MAX_TEXT_LENGTH, "%s", history->lines[*position]);
}

void history_next(TextLineHistory *history, char *line, int *position)
{
    if (history->size == 0 || *position == NOT_IN_HISTORY ) {
        return;
    }

    int new_position;
    new_position = (*position + 1) % history->size;
    if (new_position == (history->end + 1) % history->size) {
        // Do not move past the end of history
        return;
    }

    *position = new_position;
    snprintf(line, MAX_TEXT_LENGTH, "%s", history->lines[*position]);
}

TextLineHistory* current_history(LocalPlayer *local)
{
    if (local->typing_buffer[0] == CRAFT_KEY_SIGN) {
        return &local->typing_history[SIGN_HISTORY];
    } else if (local->typing_buffer[0] == CRAFT_KEY_COMMAND) {
        return &local->typing_history[COMMAND_HISTORY];
    } else if (local->typing_buffer[0] == PW_KEY_LUA) {
        return &local->typing_history[LUA_HISTORY];
    } else {
        return &local->typing_history[CHAT_HISTORY];
    }
}

void get_history_path(int history_type, int player_id, char *path)
{
    switch (history_type) {
    case CHAT_HISTORY:
        snprintf(path, MAX_PATH_LENGTH, "%s/chat%d.history",
                 config->path, player_id);
        break;
    case COMMAND_HISTORY:
        snprintf(path, MAX_PATH_LENGTH, "%s/command%d.history",
                 config->path, player_id);
        break;
    case SIGN_HISTORY:
        snprintf(path, MAX_PATH_LENGTH, "%s/sign%d.history",
                 config->path, player_id);
        break;
    case LUA_HISTORY:
        snprintf(path, MAX_PATH_LENGTH, "%s/lua%d.history",
                 config->path, player_id);
        break;
    }
}

void history_load(LocalPlayer *local)
{
    for (int h=0; h<NUM_HISTORIES; h++) {
        char history_path[MAX_PATH_LENGTH];
        TextLineHistory *history = local->typing_history + h;
        FILE *fp;
        char buf[MAX_TEXT_LENGTH];
        get_history_path(h, local->player->id, history_path);
        fp = fopen(history_path, "r");
        if (fp == NULL) {
            continue;
        }
        while (fgets(buf, MAX_TEXT_LENGTH, fp) != NULL) {
            char *p = strchr(buf, '\n');
            if (p) *p = '\0';
            history_add(history, buf);
        }
        fclose(fp);
    }
}

void history_save(LocalPlayer *local)
{
    for (int h=0; h<NUM_HISTORIES; h++) {
        char history_path[MAX_PATH_LENGTH];
        TextLineHistory *history = local->typing_history + h;
        get_history_path(h, local->player->id, history_path);
        if (history->size > 0) {
            FILE *fp;
            fp = fopen(history_path, "w");
            if (fp == NULL) {
                return;
            }
            for (int j = 0; j < history->size; j++) {
                fprintf(fp, "%s\n", history->lines[j]);
            }
            fclose(fp);
        }
    }
}

void create_menus(LocalPlayer *local)
{
    Menu *menu;
    local_player_clear_menus(local);

    // Main menu
    menu = &local->menu;
    menu_set_title(menu, "PIWORLD");
    local->menu_id_resume = menu_add(menu, "Resume");
    local->menu_id_options = menu_add(menu, "Options");
    local->menu_id_new = menu_add(menu, "New");
    local->menu_id_load = menu_add(menu, "Load");
    local->menu_id_exit = menu_add(menu, "Exit");

    // Options menu
    menu = &local->menu_options;
    menu_set_title(menu, "OPTIONS");
    local->menu_id_script = menu_add(menu, "Script");
    local->menu_id_crosshairs = menu_add_option(menu, "Crosshairs");
    local->menu_id_fullscreen = menu_add_option(menu, "Fullscreen");
    local->menu_id_verbose = menu_add_option(menu, "Verbose");
    local->menu_id_wireframe = menu_add_option(menu, "Wireframe");
    local->menu_id_worldgen = menu_add(menu, "Worldgen");
    local->menu_id_options_resume = menu_add(menu, "Resume");
    menu_set_option(menu, local->menu_id_crosshairs, config->show_crosshairs);
    menu_set_option(menu, local->menu_id_fullscreen, config->fullscreen);
    menu_set_option(menu, local->menu_id_verbose, config->verbose);
    menu_set_option(menu, local->menu_id_wireframe, config->show_wireframe);

    // New menu
    menu = &local->menu_new;
    menu_set_title(menu, "NEW GAME");
    local->menu_id_new_game_name = menu_add_line_edit(menu, "Name");
    local->menu_id_new_ok = menu_add(menu, "OK");
    local->menu_id_new_cancel = menu_add(menu, "Cancel");

    // Load menu
    menu = &local->menu_load;
    menu_set_title(menu, "LOAD GAME");

    // Item in hand menu
    menu = &local->menu_item_in_hand;
    menu_set_title(menu, "ITEM IN HAND");
    populate_texture_menu(menu);
    local->menu_id_item_in_hand_cancel = menu_add(menu, "Cancel");

    // Block edit menu
    menu = &local->menu_block_edit;
    menu_set_title(menu, "EDIT BLOCK");

    // Texture menu
    menu = &local->menu_texture;
    menu_set_title(menu, "TEXTURE");
    populate_texture_menu(menu);
    local->menu_id_texture_cancel = menu_add(menu, "Cancel");

    // Shape menu
    menu = &local->menu_shape;
    menu_set_title(menu, "SHAPE");
    populate_shape_menu(menu);
    local->menu_id_shape_cancel = menu_add(menu, "Cancel");

    // Script menu
    menu = &local->menu_script;
    menu_set_title(menu, "SCRIPT");
    local->menu_id_script_run = menu_add(menu, "Run");
    local->menu_id_script_cancel = menu_add(menu, "Cancel");

    // Run script menu
    menu = &local->menu_script_run;
    char *path = realpath(".", NULL);
    snprintf(local->menu_script_run_dir, MAX_DIR_LENGTH, path);
    free(path);
    menu_set_title(menu, "RUN SCRIPT");

    // Worldgen menu
    menu = &local->menu_worldgen;
    menu_set_title(menu, "WORLDGEN");
    local->menu_id_worldgen_select = menu_add(menu, "Select");
    local->menu_id_worldgen_cancel = menu_add(menu, "Cancel");

    // Worldgen select menu
    menu = &local->menu_worldgen_select;
    snprintf(local->menu_worldgen_dir, MAX_DIR_LENGTH, "%s/worldgen",
             get_data_dir());
    menu_set_title(menu, "SELECT WORLDGEN");
}

void populate_block_edit_menu(LocalPlayer *local, int w, char *sign_text,
    int light, int extra, int shape, int transform)
{
    Menu *menu = &local->menu_block_edit;
    menu_clear_items(menu);
    char text[MAX_TEXT_LENGTH];
    snprintf(text, MAX_TEXT_LENGTH, "Texture: %s", item_names[w - 1]);
    local->menu_id_texture = menu_add(menu, text);
    local->menu_id_sign_text = menu_add_line_edit(menu, "Sign Text");
    if (sign_text) {
        menu_set_text(menu, local->menu_id_sign_text, sign_text);
    }
    local->menu_id_light = menu_add_line_edit(menu, "Light (0-15)");
    if (light) {
        char light_text[MAX_TEXT_LENGTH];
        snprintf(light_text, MAX_TEXT_LENGTH, "%d", light);
        menu_set_text(menu, local->menu_id_light, light_text);
    }
    snprintf(text, MAX_TEXT_LENGTH, "Shape: %s", shape_names[shape]);
    local->menu_id_shape = menu_add(menu, text);
    local->menu_id_transform = menu_add_line_edit(menu, "Transform (0-7)");
    if (transform) {
        char transform_text[MAX_TEXT_LENGTH];
        snprintf(transform_text, MAX_TEXT_LENGTH, "%d", transform);
        menu_set_text(menu, local->menu_id_transform, transform_text);
    }
    local->menu_id_control_bit = menu_add_option(menu, "Control Bit");
    menu_set_option(menu, local->menu_id_control_bit, is_control(extra));
    local->menu_id_open_bit = menu_add_option(menu, "Open");
    menu_set_option(menu, local->menu_id_open_bit, is_open(extra));
    local->menu_id_block_edit_resume = menu_add(menu, "Resume");
}

void populate_texture_menu(Menu *menu)
{
    for (int i=0; i<item_count; i++) {
        menu_add(menu, (char *)item_names[i]);
    }
}

void populate_shape_menu(Menu *menu)
{
    for (int i=0; i<shape_count; i++) {
        menu_add(menu, (char *)shape_names[i]);
    }
}

void populate_load_menu(LocalPlayer *local)
{
    Menu *menu = &local->menu_load;
    menu_clear_items(menu);
    DIR *dir = opendir(config->path);
    struct dirent *dp;
    for (;;) {
        char world_name[MAX_TEXT_LENGTH];
        dp = readdir(dir);
        if (dp == NULL) {
            break;
        }
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 ||
            strlen(dp->d_name) <= 8 ||
            strcmp(dp->d_name + (strlen(dp->d_name) - 8), ".piworld") != 0) {
            continue;
        }
        snprintf(world_name, strlen(dp->d_name) - 8 + 1, "%s", dp->d_name);
        menu_add(menu, world_name);
    }
    closedir(dir);
    menu_sort(menu);
    local->menu_id_load_cancel = menu_add(menu, "Cancel");
}

void populate_script_run_menu(LocalPlayer *local)
{
    Menu *menu = &local->menu_script_run;
    menu_clear_items(menu);
    menu_add(menu, "..");
    DIR *dir = opendir(local->menu_script_run_dir);
    struct dirent *dp;
    for (;;) {
        dp = readdir(dir);
        if (dp == NULL) {
            break;
        }
        if (dp->d_name[0] == '.') {
            continue;  // ignore hidden files
        }
        char path[MAX_PATH_LENGTH];
        snprintf(path, MAX_PATH_LENGTH, "%s/%s", local->menu_script_run_dir,
                 dp->d_name);
        struct stat sb;
        stat(path, &sb);
        if (S_ISDIR(sb.st_mode)) {
            snprintf(path, MAX_PATH_LENGTH, "%s/", dp->d_name);
        } else {
            if (strlen(dp->d_name) <= 4 ||
                strcmp(dp->d_name + (strlen(dp->d_name) - 4), ".lua") != 0) {
                continue;
            }
            snprintf(path, MAX_PATH_LENGTH, "%s", dp->d_name);
        }
        menu_add(menu, path);
    }
    closedir(dir);
    menu_sort(menu);
    local->menu_id_script_run_cancel = menu_add(menu, "Cancel");
}

void populate_worldgen_select_menu(LocalPlayer *local)
{
    Menu *menu = &local->menu_worldgen_select;
    menu_clear_items(menu);
    DIR *dir = opendir(local->menu_worldgen_dir);
    struct dirent *dp;
    for (;;) {
        dp = readdir(dir);
        if (dp == NULL) {
            break;
        }
        if (dp->d_name[0] == '.') {
            continue;  // ignore hidden files
        }
        char path[MAX_PATH_LENGTH];
        char wg[MAX_PATH_LENGTH];
        snprintf(path, MAX_PATH_LENGTH, "%s/%s", local->menu_worldgen_dir,
                 dp->d_name);
        struct stat sb;
        stat(path, &sb);
        if (S_ISDIR(sb.st_mode)) {
            continue;  // ignore directories
        } else {
            if (strlen(dp->d_name) <= 4 ||
                strcmp(dp->d_name + (strlen(dp->d_name) - 4), ".lua") != 0) {
                continue;
            }
            snprintf(wg, MAX_PATH_LENGTH, "%s", dp->d_name);
            *strrchr(wg, '.') = '\0';
        }
        menu_add(menu, wg);
    }
    closedir(dir);
    menu_sort(menu);
    local->menu_id_worldgen_select_cancel = menu_add(menu, "Cancel");
}

void record_block(int x, int y, int z, int w, LocalPlayer *local) {
    memcpy(&local->block1, &local->block0, sizeof(Block));
    local->block0.x = x;
    local->block0.y = y;
    local->block0.z = z;
    local->block0.w = w;
}

void clear_block_under_crosshair(LocalPlayer *local)
{
    State *s = &local->player->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (hy > 0 && hy < 256 && is_destructable(hw)) {
        // If control block then run callback and do not remove.
        if (is_control(get_extra(hx, hy, hz))) {
            int face;
            hit_test_face(local->player, &hx, &hy, &hz, &face);
            client_control_callback(local->player->id, hx, hy, hz, face);
            pwlua_control_callback(local->player->id, hx, hy, hz, face);
            return;
        }

        // Save the block to be removed.
        int p = chunked(hx);
        int q = chunked(hz);
        Chunk *chunk = find_chunk(p, q);
        local->undo_block.x = hx;
        local->undo_block.y = hy;
        local->undo_block.z = hz;
        local->undo_block.texture = hw;
        local->undo_block.extra = get_extra(hx, hy, hz);
        local->undo_block.light = get_light(p, q, hx, hy, hz);
        local->undo_block.shape = get_shape(hx, hy, hz);
        local->undo_block.transform = get_transform(hx, hy, hz);
        if (local->undo_block.has_sign) {
            sign_list_free(&local->undo_block.signs);
        }
        local->undo_block.has_sign = 0;
        SignList *undo_signs = &local->undo_block.signs;
        for (size_t i = 0; i < chunk->signs.size; i++) {
            Sign *e = chunk->signs.data + i;
            if (e->x == hx && e->y == hy && e->z == hz) {
                if (local->undo_block.has_sign == 0) {
                    sign_list_alloc(undo_signs, 1);
                    local->undo_block.has_sign = 1;
                }
                sign_list_add(undo_signs, hx, hy, hz, e->face, e->text);
            }
        }
        local->has_undo_block = 1;

        set_block(hx, hy, hz, 0);
        record_block(hx, hy, hz, 0, local);
        if (config->verbose) {
            printf("%s cleared: x: %d y: %d z: %d w: %d\n",
                   local->player->name, hx, hy, hz, hw);
        }
        int hw_above = get_block(hx, hy + 1, hz);
        if (is_plant(hw_above)) {
            set_block(hx, hy + 1, hz, 0);
            if (config->verbose) {
                printf("%s cleared: x: %d y: %d z: %d w: %d\n",
                       local->player->name, hx, hy + 1, hz, hw_above);
            }
        }
    }
}

void set_block_under_crosshair(LocalPlayer *local)
{
    State *s = &local->player->state;
    int hx, hy, hz;
    int hw = hit_test(1, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    int hx2, hy2, hz2;
    int hw2 = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx2, &hy2, &hz2);
    if (hy2 > 0 && hy2 < 256 && is_obstacle(hw2, 0, 0)) {
        int shape = get_shape(hx2, hy2, hz2);
        int extra = get_extra(hx2, hy2, hz2);
        if (shape == LOWER_DOOR || shape == UPPER_DOOR) {
            // toggle open/close
            int p = chunked(hx2);
            int q = chunked(hz2);
            Chunk *chunk = find_chunk(p, q);
            DoorMapEntry *door = door_map_get(&chunk->doors, hx2, hy2, hz2);
            door_toggle_open(&chunk->doors, door, hx2, hy2, hz2, chunk->buffer);
            return;
        } else if (is_control(extra)) {
            open_menu(local, &local->menu);
            return;
        } else if (shape == GATE) {
            // toggle open/close
            int p = chunked(hx2);
            int q = chunked(hz2);
            Chunk *chunk = find_chunk(p, q);
            DoorMapEntry *gate = door_map_get(&chunk->doors, hx2, hy2, hz2);
            gate_toggle_open(gate, hx2, hy2, hz2, chunk->buffer);
            return;
        }
    }
    if (hy > 0 && hy < 256 && is_obstacle(hw, 0, 0)) {
        if (!player_intersects_block(2, s->x, s->y, s->z, hx, hy, hz)) {
            set_block(hx, hy, hz, items[local->item_index]);
            record_block(hx, hy, hz, items[local->item_index], local);
        }
        if (config->verbose) {
            printf("%s set: x: %d y: %d z: %d w: %d\n",
                   local->player->name, hx, hy, hz, items[local->item_index]);
        }
    }
}

void set_item_in_hand_to_item_under_crosshair(LocalPlayer *local)
{
    State *s = &local->player->state;
    int i = 0;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (hw > 0) {
        for (i = 0; i < item_count; i++) {
            if (items[i] == hw) {
                local->item_index = i;
                break;
            }
        }
        if (config->verbose) {
            printf("%s selected: x: %d y: %d z: %d w: %d\n",
                   local->player->name, hx, hy, hz, hw);
        }
    }
}

void open_menu_for_item_under_crosshair(LocalPlayer *local)
{
    State *s = &local->player->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (hw == 0) {
        open_menu(local, &local->menu_item_in_hand);
    } else {
        int p, q, x, y, z, face;
        char *sign_text;
        int i = 0;
        for (i = 0; i < item_count; i++) {
            if (items[i] == hw) {
                break;
            }
        }
        hit_test_face(local->player, &x, &y, &z, &face);
        p = chunked(x);
        q = chunked(z);
        sign_text = (char *)get_sign(p, q, x, y, z, face);
        local->edit_x = x;
        local->edit_y = y;
        local->edit_z = z;
        local->edit_face = face;
        populate_block_edit_menu(local, i + 1, sign_text,
                                 get_light(p, q, x, y, z), get_extra(x, y, z),
                                 get_shape(x, y, z), get_transform(x, y, z));
        open_menu(local, &local->menu_block_edit);
    }
}

void cycle_item_in_hand_up(LocalPlayer *player)
{
    player->item_index = (player->item_index + 1) % item_count;
}

void cycle_item_in_hand_down(LocalPlayer *player)
{
    player->item_index--;
    if (player->item_index < 0) {
        player->item_index = item_count - 1;
    }
}

void on_light(LocalPlayer *local)
{
    State *s = &local->player->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (hy > 0 && hy < 256 && is_destructable(hw)) {
        toggle_light(hx, hy, hz);
    }
}

void get_motion_vector(int flying, float sz, float sx, float rx, float ry,
    float *vx, float *vy, float *vz) {
    *vx = 0; *vy = 0; *vz = 0;
    if (!sz && !sx) {
        return;
    }
    if (flying) {
        float strafe = atan2f(sz, sx);
        float m = cosf(ry);
        float y = sinf(ry);
        if (sx) {
            if (!sz) {
                y = 0;
            }
            m = 1;
        }
        if (sz > 0) {
            y = -y;
        }
        *vx = cosf(rx + strafe) * m;
        *vy = y;
        *vz = sinf(rx + strafe) * m;
    } else {
        *vx = sx * cosf(rx) - sz * sinf(rx);
        *vz = sx * sinf(rx) + sz * cosf(rx);
    }
}

void handle_movement(double dt, LocalPlayer *local)
{
    State *s = &local->player->state;
    int stay_in_crouch = 0;
    float sz = 0;
    float sx = 0;
    if (!local->typing) {
        float m1 = dt * local->view_speed_left_right;
        float m2 = dt * local->view_speed_up_down;

        // Walking
        if (local->forward_is_pressed) sz = -local->movement_speed_forward_back;
        if (local->back_is_pressed) sz = local->movement_speed_forward_back;
        if (local->left_is_pressed) sx = -local->movement_speed_left_right;
        if (local->right_is_pressed) sx = local->movement_speed_left_right;

        // View direction
        if (local->view_left_is_pressed) s->rx -= m1;
        if (local->view_right_is_pressed) s->rx += m1;
        if (local->view_up_is_pressed) s->ry += m2;
        if (local->view_down_is_pressed) s->ry -= m2;
    }
    float vx, vy, vz;
    get_motion_vector(local->flying, sz, sx, s->rx, s->ry, &vx, &vy, &vz);
    if (!local->typing) {
        if (local->jump_is_pressed) {
            if (local->flying) {
                vy = 1;
            }
            else if (local->dy == 0) {
                local->dy = 8;
            }
        } else if (local->crouch_is_pressed) {
            if (local->flying) {
                vy = -1;
            }
            else if (local->dy == 0) {
                local->dy = -4;
            }
        } else {
            // If previously in a crouch, move to standing position
            int block_under_player_head = get_block(
                roundf(s->x), s->y, roundf(s->z));
            int block_above_player_head = get_block(
                roundf(s->x), s->y + 2, roundf(s->z));
            int shape = get_shape(roundf(s->x), s->y, roundf(s->z));
            int extra = get_extra(roundf(s->x), s->y, roundf(s->z));
            if (is_obstacle(block_under_player_head, shape, extra)) {
                shape = get_shape(roundf(s->x), s->y + 2, roundf(s->z));
                extra = get_extra(roundf(s->x), s->y + 2, roundf(s->z));
                if (is_obstacle(block_above_player_head, shape, extra)) {
                    stay_in_crouch = 1;
                } else {
                    local->dy = 8;
                }
            }
        }
    }
    float speed = 5;  // walking speed
    if (local->flying) {
        speed = 20;
    } else if (local->crouch_is_pressed || stay_in_crouch) {
        speed = 2;
    }
    int estimate = roundf(sqrtf(
        powf(vx * speed, 2) +
        powf(vy * speed + ABS(local->dy) * 2, 2) +
        powf(vz * speed, 2)) * dt * 8);
    int step = MAX(8, estimate);
    float ut = dt / step;
    vx = vx * ut * speed;
    vy = vy * ut * speed;
    vz = vz * ut * speed;
    for (int i = 0; i < step; i++) {
        if (local->flying) {
            local->dy = 0;
        }
        else {
            local->dy -= ut * 25;
            local->dy = MAX(local->dy, -250);
        }
        s->x += vx;
        s->y += vy + local->dy * ut;
        s->z += vz;
        int player_standing_height = 2;
        int player_couching_height = 1;
        int player_min_height = player_standing_height;
        if (local->crouch_is_pressed || stay_in_crouch) {
            player_min_height = player_couching_height;
        }
        if (collide(player_min_height, &s->x, &s->y, &s->z, &local->dy)) {
            local->dy = 0;
        }
    }
    if (s->y < 0) {
        s->y = highest_block(s->x, s->z) + 2;
    }
}

