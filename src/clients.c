#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chunks.h"
#include "clients.h"
#include "db.h"
#include "pw.h"

Client clients[MAX_CLIENTS];
int client_count;


void clients_reset(void)
{
    memset(clients, 0, sizeof(Client) * MAX_CLIENTS);
    client_count = 1;
    clients->id = 0;
}

Client *find_client(int id) {
    for (int i = 0; i < client_count; i++) {
        Client *client = clients + i;
        if (client->id == id) {
            return client;
        }
    }
    return 0;
}

int get_first_active_player(Client *client)
{
    int first_active_player = 0;
    for (int i=0; i<MAX_LOCAL_PLAYERS-1; i++) {
        Player *player = &client->players[i];
        if (player->is_active) {
            first_active_player = i;
            break;
        }
    }
    return first_active_player;
}

void delete_client(int id)
{
    Client *client = find_client(id);
    if (!client) {
        return;
    }
    int count = client_count;
    for (int i = 0; i<MAX_LOCAL_PLAYERS; i++) {
        Player *player = client->players + i;
        if (player->is_active) {
            del_buffer(player->buffer);
        }
    }
    Client *other = clients + (--count);
    memcpy(client, other, sizeof(Client));
    client_count = count;
}

void delete_all_players(void)
{
    for (int i = 0; i < client_count; i++) {
        Client *client = clients + i;
        for (int j = 0; j < MAX_LOCAL_PLAYERS; j++) {
            Player *player = client->players + j;
            if (player->is_active) {
                del_buffer(player->buffer);
            }
        }
    }
    client_count = 0;
}

void parse_buffer(char *buffer)
{
    #define INVALID_PLAYER_INDEX (p < 1 || p > MAX_LOCAL_PLAYERS)
    Client *local_client = clients;
    char *key;
    char *line = tokenize(buffer, "\n", &key);
    while (line) {
        int pid;
        int p;
        float px, py, pz, prx, pry;
        if (sscanf(line, "P,%d,%d,%f,%f,%f,%f,%f",
            &pid, &p, &px, &py, &pz, &prx, &pry) == 7)
        {
            if (INVALID_PLAYER_INDEX) goto next_line;
            Client *client = find_client(pid);
            if (!client && client_count < MAX_CLIENTS) {
                // Add a new client
                client = clients + client_count;
                client_count++;
                client->id = pid;
                // Initialize the players.
                for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
                    Player *player = client->players + i;
                    player->is_active = 0;
                    player->id = i + 1;
                    player->buffer = 0;
                    player->texture_index = i;
                }
            }
            if (client) {
                Player *player = &client->players[p - 1];
                if (!player->is_active) {
                    // Add remote player
                    player->is_active = 1;
                    snprintf(player->name, MAX_NAME_LENGTH, "player%d-%d",
                             pid, p);
                    update_player(player, px, py, pz, prx, pry, 1);
                    client_add_player(player->id);
                } else {
                    update_player(player, px, py, pz, prx, pry, 1);
                }
            }
            goto next_line;
        }
        float ux, uy, uz, urx, ury;
        if (sscanf(line, "U,%d,%d,%f,%f,%f,%f,%f",
            &pid, &p, &ux, &uy, &uz, &urx, &ury) == 7)
        {
            if (INVALID_PLAYER_INDEX) goto next_line;
            Player *me = local_client->players + (p-1);
            State *s = &me->state;
            local_client->id = pid;
            s->x = ux; s->y = uy; s->z = uz; s->rx = urx; s->ry = ury;
            force_chunks(me, get_float_size());
            if (uy == 0) {
                s->y = highest_block(s->x, s->z) + 2;
            }
            goto next_line;
        }

        int bp, bq, bx, by, bz, bw;
        if (sscanf(line, "B,%d,%d,%d,%d,%d,%d",
            &bp, &bq, &bx, &by, &bz, &bw) == 6)
        {
            Player *me = local_client->players;
            State *s = &me->state;
            _set_block(bp, bq, bx, by, bz, bw, 0);
            if (player_intersects_block(2, s->x, s->y, s->z, bx, by, bz)) {
                s->y = highest_block(s->x, s->z) + 2;
            }
            goto next_line;
        }
        if (sscanf(line, "e,%d,%d,%d,%d,%d,%d",
            &bp, &bq, &bx, &by, &bz, &bw) == 6)
        {
            _set_extra(bp, bq, bx, by, bz, bw, 0);
            goto next_line;
        }
        if (sscanf(line, "s,%d,%d,%d,%d,%d,%d",
            &bp, &bq, &bx, &by, &bz, &bw) == 6)
        {
            _set_shape(bp, bq, bx, by, bz, bw, 0);
            goto next_line;
        }
        if (sscanf(line, "t,%d,%d,%d,%d,%d,%d",
            &bp, &bq, &bx, &by, &bz, &bw) == 6)
        {
            _set_transform(bp, bq, bx, by, bz, bw, 0);
            goto next_line;
        }
        if (sscanf(line, "L,%d,%d,%d,%d,%d,%d",
            &bp, &bq, &bx, &by, &bz, &bw) == 6)
        {
            _set_light(bp, bq, bx, by, bz, bw);
            goto next_line;
        }
        if (sscanf(line, "X,%d,%d", &pid, &p) == 2) {
            if (INVALID_PLAYER_INDEX) goto next_line;
            Client *client = find_client(pid);
            if (client) {
                Player *player = &client->players[p - 1];
                player->is_active = 0;
            }
            goto next_line;
        }
        if (sscanf(line, "D,%d", &pid) == 1) {
            delete_client(pid);
            goto next_line;
        }
        int kp, kq, kk;
        if (sscanf(line, "K,%d,%d,%d", &kp, &kq, &kk) == 3) {
            db_set_key(kp, kq, kk);
            goto next_line;
        }
        if (sscanf(line, "R,%d,%d", &kp, &kq) == 2) {
            Chunk *chunk = find_chunk(kp, kq);
            if (chunk) {
                dirty_chunk(chunk);
            }
            goto next_line;
        }
        double elapsed;
        int day_length;
        if (sscanf(line, "E,%lf,%d", &elapsed, &day_length) == 2) {
            set_time_elapsed_and_day_length(elapsed, day_length);
            goto next_line;
        }
        if (line[0] == 'T' && line[1] == ',') {
            char *text = line + 2;
            for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
                add_message(i+1, text);
            }
            goto next_line;
        }
        char format[64];
        snprintf(
            format, sizeof(format), "N,%%d,%%d,%%%ds", MAX_NAME_LENGTH - 1);
        char name[MAX_NAME_LENGTH];
        if (sscanf(line, format, &pid, &p, name) == 3) {
            if (INVALID_PLAYER_INDEX) goto next_line;
            Client *client = find_client(pid);
            if (client) {
                strncpy(client->players[p - 1].name, name, MAX_NAME_LENGTH);
            }
            goto next_line;
        }
        char value[MAX_NAME_LENGTH];
        snprintf(
            format, sizeof(format), "O,%%%d[^,],%%%d[^,]", MAX_NAME_LENGTH - 1,
            MAX_NAME_LENGTH - 1);
        if (sscanf(line, format, name, value) == 2) {
            printf("Got option from server %s = %s\n", name, value);
            int int_value = atoi(value);
            if (strncmp(name, "show-plants", 11) == 0 &&
                (int_value == 0 || int_value == 1)) {
                if (int_value != config->show_plants) {
                    set_show_plants(int_value);
                }
            } else if (strncmp(name, "show-trees", 9) == 0 &&
                       (int_value == 0 || int_value == 1)) {
                if (int_value != config->show_trees) {
                    set_show_trees(int_value);
                }
            } else if (strncmp(name, "show-clouds", 11) == 0 &&
                       (int_value == 0 || int_value == 1)) {
                if (int_value != config->show_clouds) {
                    set_show_clouds(int_value);
                }
            } else if (strncmp(name, "worldgen", 8) == 0) {
                // Only except a named worldgen or empty for default.
                // Only a worldgen script under the client's ./worldgen dir
                // will be excepted.
                if (strlen(value) > 0) {
                    if (strchr(value, '/')) {
                        printf(
                "Path component not allowed in worldgen from server: %s\n"
                "Please ask the server admin to use named worldgens only.\n",
                               value);
                        goto next_line;
                    }
                    set_worldgen(value);
                } else {
                    set_worldgen(NULL);
                }
            }
            goto next_line;
        }
        snprintf(
            format, sizeof(format),
            "S,%%d,%%d,%%d,%%d,%%d,%%d,%%%d[^\n]", MAX_SIGN_LENGTH - 1);
        int face;
        char text[MAX_SIGN_LENGTH] = {0};
        if (sscanf(line, format,
            &bp, &bq, &bx, &by, &bz, &face, text) >= 6)
        {
            _set_sign(bp, bq, bx, by, bz, face, text, 0);
            goto next_line;
        }

next_line:
        line = tokenize(NULL, "\n", &key);
    }
}
