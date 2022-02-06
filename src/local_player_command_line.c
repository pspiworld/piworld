#include <math.h>
#include <stdio.h>
#include <string.h>
#include "action.h"
#include "chunks.h"
#include "client.h"
#include "action.h"
#include "db.h"
#include "item.h"
#include "local_player.h"
#include "local_player_command_line.h"
#include "pg.h"
#include "pw.h"
#include "util.h"
#include "x11_event_handler.h"

void builder_block(int x, int y, int z, int w) {
    if (y <= 0 || y >= 256) {
        return;
    }
    if (is_destructable(get_block(x, y, z))) {
        set_block(x, y, z, 0);
    }
    if (w) {
        set_block(x, y, z, w);
    }
}

void copy(LocalPlayer *local) {
    memcpy(&local->copy0, &local->block0, sizeof(Block));
    memcpy(&local->copy1, &local->block1, sizeof(Block));
}

void paste(LocalPlayer *local) {
    Block *c1 = &local->copy1;
    Block *c2 = &local->copy0;
    Block *p1 = &local->block1;
    Block *p2 = &local->block0;
    int scx = SIGN(c2->x - c1->x);
    int scz = SIGN(c2->z - c1->z);
    int spx = SIGN(p2->x - p1->x);
    int spz = SIGN(p2->z - p1->z);
    int oy = p1->y - c1->y;
    int dx = ABS(c2->x - c1->x);
    int dz = ABS(c2->z - c1->z);
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x <= dx; x++) {
            for (int z = 0; z <= dz; z++) {
                int w = get_block(c1->x + x * scx, y, c1->z + z * scz);
                builder_block(p1->x + x * spx, y + oy, p1->z + z * spz, w);
            }
        }
    }
}

void array(Block *b1, Block *b2, int xc, int yc, int zc) {
    if (b1->w != b2->w) {
        return;
    }
    int w = b1->w;
    int dx = b2->x - b1->x;
    int dy = b2->y - b1->y;
    int dz = b2->z - b1->z;
    xc = dx ? xc : 1;
    yc = dy ? yc : 1;
    zc = dz ? zc : 1;
    for (int i = 0; i < xc; i++) {
        int x = b1->x + dx * i;
        for (int j = 0; j < yc; j++) {
            int y = b1->y + dy * j;
            for (int k = 0; k < zc; k++) {
                int z = b1->z + dz * k;
                builder_block(x, y, z, w);
            }
        }
    }
}

void cube(Block *b1, Block *b2, int fill) {
    if (b1->w != b2->w) {
        return;
    }
    int w = b1->w;
    int x1 = MIN(b1->x, b2->x);
    int y1 = MIN(b1->y, b2->y);
    int z1 = MIN(b1->z, b2->z);
    int x2 = MAX(b1->x, b2->x);
    int y2 = MAX(b1->y, b2->y);
    int z2 = MAX(b1->z, b2->z);
    int a = (x1 == x2) + (y1 == y2) + (z1 == z2);
    for (int x = x1; x <= x2; x++) {
        for (int y = y1; y <= y2; y++) {
            for (int z = z1; z <= z2; z++) {
                if (!fill) {
                    int n = 0;
                    n += x == x1 || x == x2;
                    n += y == y1 || y == y2;
                    n += z == z1 || z == z2;
                    if (n <= a) {
                        continue;
                    }
                }
                builder_block(x, y, z, w);
            }
        }
    }
}

void fence(Block *b1, Block *b2) {
    if (b1->w != b2->w) {
        return;
    }
    int w = b1->w;
    int x1 = MIN(b1->x, b2->x);
    int y1 = MIN(b1->y, b2->y);
    int z1 = MIN(b1->z, b2->z);
    int x2 = MAX(b1->x, b2->x);
    int y2 = MAX(b1->y, b2->y);
    int z2 = MAX(b1->z, b2->z);
    int a = (x1 == x2) + (y1 == y2) + (z1 == z2);
    for (int x = x1; x <= x2; x++) {
        for (int y = y1; y <= y2; y++) {
            for (int z = z1; z <= z2; z++) {
                int n = 0;
                n += x == x1 || x == x2;
                n += y == y1 || y == y2;
                n += z == z1 || z == z2;
                if (n <= a) {
                    continue;
                }
                if (x > x1 && x < x2 && z > z1 && z < z2) {
                    continue;
                }
                builder_block(x, y, z, w);
                int shape = FENCE;
                if (x == x1 && z == z1) {
                    shape = FENCE_L;  // corner
                    set_transform(x, y, z, 2);
                } else if (x == x2 && z == z1) {
                    shape = FENCE_L;  // corner
                } else if (x == x2 && z == z2) {
                    shape = FENCE_L;  // corner
                    set_transform(x, y, z, 1);
                } else if (x == x1 && z == z2) {
                    shape = FENCE_L;  // corner
                    set_transform(x, y, z, 3);
                } else if (x != x1 && x != x2) {
                    // other two sides of the fence
                    set_transform(x, y, z, 1);
                }
                set_shape(x, y, z, shape);
            }
        }
    }
}

void sphere(Block *center, int radius, int fill, int fx, int fy, int fz) {
    static const float offsets[8][3] = {
        {-0.5, -0.5, -0.5},
        {-0.5, -0.5, 0.5},
        {-0.5, 0.5, -0.5},
        {-0.5, 0.5, 0.5},
        {0.5, -0.5, -0.5},
        {0.5, -0.5, 0.5},
        {0.5, 0.5, -0.5},
        {0.5, 0.5, 0.5}
    };
    int cx = center->x;
    int cy = center->y;
    int cz = center->z;
    int w = center->w;
    for (int x = cx - radius; x <= cx + radius; x++) {
        if (fx && x != cx) {
            continue;
        }
        for (int y = cy - radius; y <= cy + radius; y++) {
            if (fy && y != cy) {
                continue;
            }
            for (int z = cz - radius; z <= cz + radius; z++) {
                if (fz && z != cz) {
                    continue;
                }
                int inside = 0;
                int outside = fill;
                for (int i = 0; i < 8; i++) {
                    float dx = x + offsets[i][0] - cx;
                    float dy = y + offsets[i][1] - cy;
                    float dz = z + offsets[i][2] - cz;
                    float d = sqrtf(dx * dx + dy * dy + dz * dz);
                    if (d < radius) {
                        inside = 1;
                    }
                    else {
                        outside = 1;
                    }
                }
                if (inside && outside) {
                    builder_block(x, y, z, w);
                }
            }
        }
    }
}

void cylinder(Block *b1, Block *b2, int radius, int fill) {
    if (b1->w != b2->w) {
        return;
    }
    int w = b1->w;
    int x1 = MIN(b1->x, b2->x);
    int y1 = MIN(b1->y, b2->y);
    int z1 = MIN(b1->z, b2->z);
    int x2 = MAX(b1->x, b2->x);
    int y2 = MAX(b1->y, b2->y);
    int z2 = MAX(b1->z, b2->z);
    int fx = x1 != x2;
    int fy = y1 != y2;
    int fz = z1 != z2;
    if (fx + fy + fz != 1) {
        return;
    }
    Block block = {x1, y1, z1, w};
    if (fx) {
        for (int x = x1; x <= x2; x++) {
            block.x = x;
            sphere(&block, radius, fill, 1, 0, 0);
        }
    }
    if (fy) {
        for (int y = y1; y <= y2; y++) {
            block.y = y;
            sphere(&block, radius, fill, 0, 1, 0);
        }
    }
    if (fz) {
        for (int z = z1; z <= z2; z++) {
            block.z = z;
            sphere(&block, radius, fill, 0, 0, 1);
        }
    }
}

void tree(Block *block) {
    int bx = block->x;
    int by = block->y;
    int bz = block->z;
    for (int y = by + 3; y < by + 8; y++) {
        for (int dx = -3; dx <= 3; dx++) {
            for (int dz = -3; dz <= 3; dz++) {
                int dy = y - (by + 4);
                int d = (dx * dx) + (dy * dy) + (dz * dz);
                if (d < 11) {
                    builder_block(bx + dx, y, bz + dz, 15);
                }
            }
        }
    }
    for (int y = by; y < by + 7; y++) {
        builder_block(bx, y, bz, 5);
    }
}

void parse_command(LocalPlayer *local, const char *buffer, int forward) {
    char server_addr[MAX_ADDR_LENGTH];
    int server_port = DEFAULT_PORT;
    char filename[MAX_FILENAME_LENGTH];
    char name[MAX_NAME_LENGTH];
    int int_option, radius, count, p, q, xc, yc, zc;
    char window_title[MAX_TITLE_LENGTH];
    char path[MAX_PATH_LENGTH];
    Player *player = local->player;
    if (strcmp(buffer, "/fullscreen") == 0) {
        pg_toggle_fullscreen();
    }
    else if (sscanf(buffer,
        "/online %128s %d", server_addr, &server_port) >= 1) {
        pw_connect_to_server(server_addr, server_port);
    }
    else if (sscanf(buffer, "/offline %128s", filename) == 1) {
        snprintf(path, MAX_PATH_LENGTH, "%s/%s.piworld", config->path,
                 filename);
        pw_load_game(path);
    }
    else if (strcmp(buffer, "/offline") == 0) {
        get_default_db_path(path);
        pw_load_game(path);
    }
    else if (sscanf(buffer, "/nick %32c", name) == 1) {
        int prefix_length = strlen("/nick ");
        name[MIN(strlen(buffer) - prefix_length, MAX_NAME_LENGTH-1)] = '\0';
        strncpy(player->name, name, MAX_NAME_LENGTH);
        client_nick(player->id, name);
    }
    else if (strcmp(buffer, "/spawn") == 0) {
        client_spawn(player->id);
    }
    else if (strcmp(buffer, "/goto") == 0) {
        client_goto(player->id, "");
    }
    else if (sscanf(buffer, "/goto %32c", name) == 1) {
        client_goto(player->id, name);
    }
    else if (sscanf(buffer, "/pq %d %d", &p, &q) == 2) {
        client_pq(player->id, p, q);
    }
    else if (sscanf(buffer, "/view %d", &radius) == 1) {
        if (radius >= 0 && radius <= 24) {
            set_render_radius(radius);
        }
        else {
            add_message(player->id,
                        "Viewing distance must be between 0 and 24.");
        }
    }
    else if (sscanf(buffer, "/players %d", &int_option) == 1) {
        if (int_option >= 1 && int_option <= MAX_LOCAL_PLAYERS) {
            if (config->players != int_option) {
                change_player_count(int_option);
            }
        }
        else {
            add_message(player->id, "Player count must be between 1 and 4.");
        }
    }
    else if (strcmp(buffer, "/exit") == 0) {
        if (config->players <= 1) {
            pw_exit();
        } else {
            remove_player(local);
        }
    }
    else if (sscanf(buffer, "/position %d %d %d", &xc, &yc, &zc) == 3) {
        State *s = &player->state;
        s->x = xc;
        s->y = yc;
        s->z = zc;
    }
    else if (sscanf(buffer, "/show-chat-text %d", &int_option) == 1) {
        config->show_chat_text = int_option;
    }
    else if (sscanf(buffer, "/show-clouds %d", &int_option) == 1) {
        if (!is_online()) {
            set_show_clouds(int_option);
        } else {
            printf("Cannot change worldgen when connected to server.\n");
        }
    }
    else if (sscanf(buffer, "/show-crosshairs %d", &int_option) == 1) {
        config->show_crosshairs = int_option;
    }
    else if (sscanf(buffer, "/show-item %d", &int_option) == 1) {
        config->show_item = int_option;
    }
    else if (sscanf(buffer, "/show-info-text %d", &int_option) == 1) {
        config->show_info_text = int_option;
    }
    else if (sscanf(buffer, "/show-lights %d", &int_option) == 1) {
        if (!is_online()) {
            set_show_lights(int_option);
        } else {
            printf("Cannot change worldgen when connected to server.\n");
        }
    }
    else if (sscanf(buffer, "/show-plants %d", &int_option) == 1) {
        if (!is_online()) {
            set_show_plants(int_option);
        } else {
            printf("Cannot change worldgen when connected to server.\n");
        }
    }
    else if (sscanf(buffer, "/show-player-names %d", &int_option) == 1) {
        config->show_player_names = int_option;
    }
    else if (sscanf(buffer, "/show-trees %d", &int_option) == 1) {
        if (!is_online()) {
            set_show_trees(int_option);
        } else {
            printf("Cannot change worldgen when connected to server.\n");
        }
    }
    else if (strcmp(buffer, "/worldgen") == 0) {
        if (!is_online()) {
            set_worldgen(NULL);
            db_set_option("worldgen", "");
        } else {
            printf("Cannot change worldgen when connected to server.\n");
        }
    }
    else if (sscanf(buffer, "/worldgen %512c", path) == 1) {
        if (!is_online()) {
            int prefix_length = strlen("/worldgen ");;
            path[strlen(buffer) - prefix_length] = '\0';
            set_worldgen(path);
            db_set_option("worldgen", path);
        } else {
            printf("Cannot change worldgen when connected to server.\n");
        }
    }
    else if (sscanf(buffer, "/show-wireframe %d", &int_option) == 1) {
        config->show_wireframe = int_option;
    }
    else if (sscanf(buffer, "/verbose %d", &int_option) == 1) {
        config->verbose = int_option;
    }
    else if (sscanf(buffer, "/vsync %d", &int_option) == 1) {
        config->vsync = int_option;
        pg_swap_interval(config->vsync);
    }
    else if (sscanf(buffer, "/window-size %d %d", &xc, &yc) == 2) {
        pg_resize_window(xc, yc);
    }
    else if (sscanf(buffer, "/window-title %128c", window_title) == 1) {
        int prefix_length = strlen("/window-title ");;
        window_title[strlen(buffer) - prefix_length] = '\0';
        pg_set_window_title(window_title);
    }
    else if (sscanf(buffer, "/window-xy %d %d", &xc, &yc) == 2) {
        pg_move_window(xc, yc);
    }
    else if (strcmp(buffer, "/copy") == 0) {
        copy(local);
    }
    else if (strcmp(buffer, "/paste") == 0) {
        paste(local);
    }
    else if (strcmp(buffer, "/tree") == 0) {
        tree(&local->block0);
    }
    else if (sscanf(buffer, "/array %d %d %d", &xc, &yc, &zc) == 3) {
        array(&local->block1, &local->block0, xc, yc, zc);
    }
    else if (sscanf(buffer, "/array %d", &count) == 1) {
        array(&local->block1, &local->block0, count, count, count);
    }
    else if (strcmp(buffer, "/fcube") == 0) {
        cube(&local->block0, &local->block1, 1);
    }
    else if (strcmp(buffer, "/cube") == 0) {
        cube(&local->block0, &local->block1, 0);
    }
    else if (sscanf(buffer, "/fsphere %d", &radius) == 1) {
        sphere(&local->block0, radius, 1, 0, 0, 0);
    }
    else if (sscanf(buffer, "/sphere %d", &radius) == 1) {
        sphere(&local->block0, radius, 0, 0, 0, 0);
    }
    else if (sscanf(buffer, "/fcirclex %d", &radius) == 1) {
        sphere(&local->block0, radius, 1, 1, 0, 0);
    }
    else if (sscanf(buffer, "/circlex %d", &radius) == 1) {
        sphere(&local->block0, radius, 0, 1, 0, 0);
    }
    else if (sscanf(buffer, "/fcircley %d", &radius) == 1) {
        sphere(&local->block0, radius, 1, 0, 1, 0);
    }
    else if (sscanf(buffer, "/circley %d", &radius) == 1) {
        sphere(&local->block0, radius, 0, 0, 1, 0);
    }
    else if (sscanf(buffer, "/fcirclez %d", &radius) == 1) {
        sphere(&local->block0, radius, 1, 0, 0, 1);
    }
    else if (sscanf(buffer, "/circlez %d", &radius) == 1) {
        sphere(&local->block0, radius, 0, 0, 0, 1);
    }
    else if (sscanf(buffer, "/fcylinder %d", &radius) == 1) {
        cylinder(&local->block0, &local->block1, radius, 1);
    }
    else if (sscanf(buffer, "/cylinder %d", &radius) == 1) {
        cylinder(&local->block0, &local->block1, radius, 0);
    }
    else if (strcmp(buffer, "/fence") == 0) {
        fence(&local->block0, &local->block1);
    }
    else if (sscanf(buffer, "/delete-radius %d", &radius) == 1) {
        set_delete_radius(radius);
    }
    else if (sscanf(buffer, "/time %d", &int_option) == 1) {
        if (!is_online() && int_option >= 0 && int_option <= 24) {
            pw_set_time(int_option);
        }
    }
    else if (sscanf(buffer, "/bind %512s", path) == 1) {
        action_apply_bindings(local, path);
    }
    else if (forward) {
        client_talk(buffer);
    }
}

