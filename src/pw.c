#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <libgen.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "chunk.h"
#include "chunks.h"
#include "client.h"
#include "clients.h"
#include "config.h"
#include "cube.h"
#include "db.h"
#include "door.h"
#include "item.h"
#include "local_player.h"
#include "local_players.h"
#include "map.h"
#include "matrix.h"
#include "pg.h"
#include "pw.h"
#include "pwlua.h"
#include "pwlua_worldgen.h"
#include "render.h"
#include "ring.h"
#include "sign.h"
#include "tinycthread.h"
#include "ui.h"
#include "util.h"
#include "vt.h"
#include "world.h"
#include "x11_event_handler.h"

#define MODE_OFFLINE 0
#define MODE_ONLINE 1

mtx_t edit_ring_mtx;

typedef struct {
    Worker workers[MAX_WORKERS];
    int create_radius;
    int render_radius;
    int delete_radius;
    int sign_radius;
    int width;
    int height;
    float scale;
    int ortho;
    float fov;
    int suppress_char;
    int mode;
    int mode_changed;
    int render_option_changed;
    char db_path[MAX_PATH_LENGTH];
    int time_changed;
    int gl_float_type;
    size_t float_size;
    int use_lua_worldgen;
    Ring edit_ring;
} Model;

static Model model;
static Model *g = &model;

static int prev_width, prev_height;
static int prev_player_count;

void pw_init(void)
{
    prev_width = 0;
    prev_height = 0;
    prev_player_count = 0;
    if (config->use_hfloat) {
        g->gl_float_type = GL_HALF_FLOAT_OES;
        g->float_size = sizeof(hfloat);
    } else {
        g->gl_float_type = GL_FLOAT;
        g->float_size = sizeof(GLfloat);
    }

    g->width = config->window_width;
    g->height = config->window_height;

    // ONLINE STATUS //
    if (strlen(config->server) > 0) {
        g->mode = MODE_ONLINE;
        get_server_db_cache_path(g->db_path);
    } else {
        g->mode = MODE_OFFLINE;
        snprintf(g->db_path, MAX_PATH_LENGTH, "%s", config->db_path);
    }

    mtx_init(&edit_ring_mtx, mtx_plain);
}

void pw_deinit(void)
{
    mtx_destroy(&edit_ring_mtx);
    if (g->use_lua_worldgen == 1) {
        pwlua_worldgen_deinit();
    }
}

size_t get_float_size(void)
{
    return g->float_size;
}

char *get_db_path(void)
{
    return g->db_path;
}

void pw_setup_window(void)
{
    g->scale = get_scale_factor();
    pg_get_window_size(&g->width, &g->height);
    glViewport(0, 0, g->width, g->height);
    if (prev_width != g->width || prev_height != g->height ||
        prev_player_count != config->players) {
        set_players_view_size(g->width, g->height);
        prev_width = g->width;
        prev_height = g->height;
        prev_player_count = config->players;;
    }
}

int check_time_changed(void)
{
    int changed = g->time_changed;
    g->time_changed = 0;
    return changed;
}

int check_render_option_changed(void)
{
    int changed = g->render_option_changed;
    g->render_option_changed = 0;
    return changed;
}

int check_mode_changed(void)
{
    int changed = g->mode_changed;
    g->mode_changed = 0;
    return changed;
}

void pw_unload_game(void)
{
    db_clear_state();
    db_clear_player_names();
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = &local_players[i];
        State *ls = &local->player->state;
        db_save_state(ls->x, ls->y, ls->z, ls->rx, ls->ry);
        db_save_player_name(local_players[i].player->name);
        history_save(local);
    }
    char time_str[16];
    snprintf(time_str, 16, "%f", time_of_day());
    db_set_option("time", time_str);
    db_close();
    db_disable();
    client_stop();
    client_disable();
    delete_all_chunks();
    delete_all_players();
    ring_free(&g->edit_ring);
    set_worldgen(NULL);
}

int get_delete_radius(void)
{
    return g->delete_radius;
}

void set_player_count(Client *client, int count)
{
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        Player *player = client->players + i;
        if (i < count) {
            player->is_active = 1;
        } else {
            player->is_active = 0;
        }
    }
}

int get_next_local_player(Client *client, int start)
{
    int next = get_first_active_player(client);

    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        Player *player = &client->players[i];
        if (i > start && player->is_active) {
            next = i;
            break;
        }
    }
    return next;
}

void get_next_player(int *player_index, int *client_id)
{
    Client *client = find_client(*client_id);
    if (!client) {
        client = clients;
        *client_id = client->id;
        *player_index = 1;
    }
    int next = get_next_local_player(client, *player_index);
    if (next <= *player_index) {
        *client_id = (*client_id + 1) % (client_count + 1);
        if (*client_id == 0) {
            *client_id = 1;
        }
        client = find_client(*client_id);
        if (!client) {
            client = clients;
            *client_id = client->id;
        }
        next = get_first_active_player(client);
    }
    *player_index = next;
}

float get_scale_factor(void)
{
    return 1.0;
}

void get_sight_vector(float rx, float ry, float *vx, float *vy, float *vz)
{
    float m = cosf(ry);
    *vx = cosf(rx - RADIANS(90)) * m;
    *vy = sinf(ry);
    *vz = sinf(rx - RADIANS(90)) * m;
}

GLuint gen_player_buffer(float x, float y, float z, float rx, float ry, int p)
{
    GLfloat *data = malloc_faces(10, 6, sizeof(GLfloat));
    make_player(data, x, y, z, rx, ry, p);
    return gen_faces(10, 6, data, sizeof(GLfloat));
}

void update_player(Player *player,
    float x, float y, float z, float rx, float ry, int interpolate)
{
    if (interpolate) {
        State *s1 = &player->state1;
        State *s2 = &player->state2;
        memcpy(s1, s2, sizeof(State));
        s2->x = x; s2->y = y; s2->z = z; s2->rx = rx; s2->ry = ry;
        s2->t = pg_get_time();
        if (s2->rx - s1->rx > PI) {
            s1->rx += 2 * PI;
        }
        if (s1->rx - s2->rx > PI) {
            s1->rx -= 2 * PI;
        }
    }
    else {
        State *s = &player->state;
        s->x = x; s->y = y; s->z = z; s->rx = rx; s->ry = ry;
        del_buffer(player->buffer);
        player->buffer = gen_player_buffer(s->x, s->y, s->z, s->rx, s->ry,
                                           player->texture_index);
    }
}

void interpolate_player(Player *player)
{
    State *s1 = &player->state1;
    State *s2 = &player->state2;
    float t1 = s2->t - s1->t;
    float t2 = pg_get_time() - s2->t;
    t1 = MIN(t1, 1);
    t1 = MAX(t1, 0.1);
    float p = MIN(t2 / t1, 1);
    update_player(
        player,
        s1->x + (s2->x - s1->x) * p,
        s1->y + (s2->y - s1->y) * p,
        s1->z + (s2->z - s1->z) * p,
        s1->rx + (s2->rx - s1->rx) * p,
        s1->ry + (s2->ry - s1->ry) * p,
        0);
}

float player_player_distance(Player *p1, Player *p2)
{
    State *s1 = &p1->state;
    State *s2 = &p2->state;
    float x = s2->x - s1->x;
    float y = s2->y - s1->y;
    float z = s2->z - s1->z;
    return sqrtf(x * x + y * y + z * z);
}

float player_crosshair_distance(Player *p1, Player *p2)
{
    State *s1 = &p1->state;
    State *s2 = &p2->state;
    float d = player_player_distance(p1, p2);
    float vx, vy, vz;
    get_sight_vector(s1->rx, s1->ry, &vx, &vy, &vz);
    vx *= d; vy *= d; vz *= d;
    float px, py, pz;
    px = s1->x + vx; py = s1->y + vy; pz = s1->z + vz;
    float x = s2->x - px;
    float y = s2->y - py;
    float z = s2->z - pz;
    return sqrtf(x * x + y * y + z * z);
}

Player *player_crosshair(Player *player)
{
    Player *result = 0;
    float threshold = RADIANS(5);
    float best = 0;
    for (int i = 0; i < client_count; i++) {
        Client *client = clients + i;
        for (int j = 0; j < MAX_LOCAL_PLAYERS; j++) {
            Player *other = client->players + j;
            if (other == player || !other->is_active) {
                continue;
            }
            float p = player_crosshair_distance(player, other);
            float d = player_player_distance(player, other);
            if (d < 96 && p / d < threshold) {
                if (best == 0 || d < best) {
                    best = d;
                    result = other;
                }
            }
        }
    }
    return result;
}

void gen_sign_chunk_buffer(Chunk *chunk)
{
    SignList *signs = &chunk->signs;

    // first pass - count characters
    int max_faces = 0;
    for (size_t i = 0; i < signs->size; i++) {
        Sign *e = signs->data + i;
        max_faces += strlen(e->text);
    }

    // second pass - generate geometry
    GLfloat *data = malloc_faces_with_rgba(5, max_faces);
    int faces = 0;
    for (size_t i = 0; i < signs->size; i++) {
        Sign *e = signs->data + i;
        int shape = get_shape(e->x, e->y, e->z);
        float y_face_height = item_height(shape);
        faces += gen_sign_buffer(
            data + faces * 54, e->x, e->y, e->z, e->face, e->text,
            y_face_height);
    }

    del_buffer(chunk->sign_buffer);
    chunk->sign_buffer = gen_faces_with_rgba(5, faces, data);
    chunk->sign_faces = faces;
    chunk->dirty_signs = 0;
}

void map_set_func(int x, int y, int z, int w, void *arg)
{
    Map *map = (Map *)arg;
    map_set(map, x, y, z, w);
}

void check_workers(void)
{
    for (int i = 0; i < config->worker_count; i++) {
        Worker *worker = g->workers + i;
        mtx_lock(&worker->mtx);
        if (worker->state == WORKER_DONE) {
            WorkerItem *item = &worker->item;
            Chunk *chunk = find_chunk(item->p, item->q);
            if (chunk) {
                if (item->load) {
                    Map *block_map = item->block_maps[1][1];
                    Map *extra_map = item->extra_maps[1][1];
                    Map *light_map = item->light_maps[1][1];
                    Map *shape_map = item->shape_maps[1][1];
                    Map *transform_map = item->transform_maps[1][1];
                    map_free(&chunk->map);
                    map_free(&chunk->extra);
                    map_free(&chunk->lights);
                    map_free(&chunk->shape);
                    map_free(&chunk->transform);
                    sign_list_free(&chunk->signs);
                    map_copy(&chunk->map, block_map);
                    map_copy(&chunk->extra, extra_map);
                    map_copy(&chunk->lights, light_map);
                    map_copy(&chunk->shape, shape_map);
                    map_copy(&chunk->transform, transform_map);
                    sign_list_copy(&chunk->signs, &item->signs);
                    sign_list_free(&item->signs);
                    request_chunk(item->p, item->q);
                }

                // DoorMap data copy is required whether the doors were added
                // from loading game data or generated from the worldgen.
                DoorMap *door_map = item->door_maps[1][1];
                door_map_free(&chunk->doors);
                door_map_copy(&chunk->doors, door_map);

                generate_chunk(chunk, item, g->float_size);
            }
            for (int a = 0; a < 3; a++) {
                for (int b = 0; b < 3; b++) {
                    Map *block_map = item->block_maps[a][b];
                    Map *extra_map = item->extra_maps[a][b];
                    Map *light_map = item->light_maps[a][b];
                    Map *shape_map = item->shape_maps[a][b];
                    Map *transform_map = item->transform_maps[a][b];
                    DoorMap *door_map = item->door_maps[a][b];
                    if (block_map) {
                        map_free(block_map);
                        free(block_map);
                    }
                    if (extra_map) {
                        map_free(extra_map);
                        free(extra_map);
                    }
                    if (light_map) {
                        map_free(light_map);
                        free(light_map);
                    }
                    if (shape_map) {
                        map_free(shape_map);
                        free(shape_map);
                    }
                    if (transform_map) {
                        map_free(transform_map);
                        free(transform_map);
                    }
                    if (door_map) {
                        door_map_free(door_map);
                        free(door_map);
                    }
                }
            }
            worker->state = WORKER_IDLE;
        }
        mtx_unlock(&worker->mtx);
    }
}

void ensure_chunks(Player *player)
{
    check_workers();
    force_chunks(player, g->float_size);
    for (int i = 0; i < config->worker_count; i++) {
        Worker *worker = g->workers + i;
        mtx_lock(&worker->mtx);
        if (worker->state == WORKER_IDLE) {
            ensure_chunks_worker(player, worker, g->width, g->height, g->fov, g->ortho, g->render_radius, g->create_radius);
        }
        mtx_unlock(&worker->mtx);
    }
}

int worker_run(void *arg)
{
    Worker *worker = (Worker *)arg;
    lua_State *L = NULL;
    if (g->use_lua_worldgen == 1) {
        L = pwlua_worldgen_new_generator();
    }
    while (!worker->exit_requested) {
        mtx_lock(&worker->mtx);
        while (worker->state != WORKER_BUSY) {
            cnd_wait(&worker->cnd, &worker->mtx);
            if (worker->exit_requested) {
                if (L != NULL) {
                    lua_close(L);
                }
                thrd_exit(1);
            }
        }
        mtx_unlock(&worker->mtx);
        WorkerItem *item = &worker->item;
        if (item->load) {
            load_chunk(item, L);
        }
        compute_chunk(item);
        mtx_lock(&worker->mtx);
        worker->state = WORKER_DONE;
        mtx_unlock(&worker->mtx);
    }
    if (L != NULL) {
        lua_close(L);
    }
    return 0;
}

void worldgen_set_sign(int x, int y, int z, int face, const char *text,
                       SignList *sign_list)
{
    sign_list_add(sign_list, x, y, z, face, text);
}

static int file_readable(const char *filename)
{
    FILE *f = fopen(filename, "r");  /* try to open file */
    if (f == NULL) return 0;  /* open failed */
    fclose(f);
    return 1;
}

void set_worldgen(char *worldgen)
{
    if (g->use_lua_worldgen) {
        pwlua_worldgen_deinit();
        config->worldgen_path[0] = '\0';
        g->use_lua_worldgen = 0;
    }
    if (worldgen && strlen(worldgen) > 0) {
        char wg_path[MAX_PATH_LENGTH];
        if (!file_readable(worldgen)) {
            snprintf(wg_path, MAX_PATH_LENGTH, "%s/worldgen/%s.lua",
                     get_data_dir(), worldgen);
        } else {
            snprintf(wg_path, MAX_PATH_LENGTH, "%s", worldgen);
        }
        if (!file_readable(wg_path)) {
            printf("Worldgen file not found: %s\n", wg_path);
            return;
        }
        strncpy(config->worldgen_path, wg_path, sizeof(config->worldgen_path));
        config->worldgen_path[sizeof(config->worldgen_path)-1] = '\0';
        pwlua_worldgen_init(config->worldgen_path);
        g->use_lua_worldgen = 1;
    }
    g->render_option_changed = 1;
}

void queue_set_block(int x, int y, int z, int w)
{
    int p = chunked(x);
    int q = chunked(z);
    mtx_lock(&edit_ring_mtx);
    ring_put_block(&g->edit_ring, p, q, x, y, z, w);
    mtx_unlock(&edit_ring_mtx);
}

void queue_set_extra(int x, int y, int z, int w)
{
    int p = chunked(x);
    int q = chunked(z);
    mtx_lock(&edit_ring_mtx);
    ring_put_extra(&g->edit_ring, p, q, x, y, z, w);
    mtx_unlock(&edit_ring_mtx);
}

void queue_set_light(int x, int y, int z, int w)
{
    int p = chunked(x);
    int q = chunked(z);
    mtx_lock(&edit_ring_mtx);
    ring_put_light(&g->edit_ring, p, q, x, y, z, w);
    mtx_unlock(&edit_ring_mtx);
}

void queue_set_shape(int x, int y, int z, int w)
{
    int p = chunked(x);
    int q = chunked(z);
    mtx_lock(&edit_ring_mtx);
    ring_put_shape(&g->edit_ring, p, q, x, y, z, w);
    mtx_unlock(&edit_ring_mtx);
}

void queue_set_sign(int x, int y, int z, int face, const char *text)
{
    int p = chunked(x);
    int q = chunked(z);
    mtx_lock(&edit_ring_mtx);
    ring_put_sign(&g->edit_ring, p, q, x, y, z, face, text);
    mtx_unlock(&edit_ring_mtx);
}

void queue_set_transform(int x, int y, int z, int w)
{
    int p = chunked(x);
    int q = chunked(z);
    mtx_lock(&edit_ring_mtx);
    ring_put_transform(&g->edit_ring, p, q, x, y, z, w);
    mtx_unlock(&edit_ring_mtx);
}

void add_message(int player_id, const char *text)
{
    if (player_id < 1 || player_id > MAX_LOCAL_PLAYERS) {
        printf("Message for invalid player %d: %s\n", player_id, text);
        return;
    }
    LocalPlayer *local = get_local_player(player_id - 1);
    printf("%d: %s\n", player_id, text);
    snprintf(
        local->messages[local->message_index], MAX_TEXT_LENGTH, "%s", text);
    local->message_index = (local->message_index + 1) % MAX_MESSAGES;
}

void login(void)
{
    printf("Logging in anonymously\n");
    client_login("", "");
}

void toggle_observe_view(LocalPlayer *p)
{
    int start = ((p->observe1 == 0) ?  p->player->id : p->observe1) - 1;
    if (p->observe1_client_id == 0) {
        p->observe1_client_id = clients->id;
    }
    get_next_player(&start, &p->observe1_client_id);
    p->observe1 = start + 1;
    if (p->observe1 == p->player->id &&
        p->observe1_client_id == clients->id) {
        // cancel observing of another player
        p->observe1 = 0;
        p->observe1_client_id = 0;
    }
}

void toggle_picture_in_picture_observe_view(LocalPlayer *p)
{
    int start = ((p->observe2 == 0) ?  p->player->id : p->observe2) - 1;
    if (p->observe2_client_id == 0) {
        p->observe2_client_id = clients->id;
    }
    get_next_player(&start, &p->observe2_client_id);
    p->observe2 = start + 1;
    if (p->observe2 == p->player->id &&
        p->observe2_client_id == clients->id) {
        // cancel observing of another player
        p->observe2 = 0;
        p->observe2_client_id = 0;
    }
}

int pw_get_crosshair(int player_id, int *x, int *y, int *z, int *face)
{
    if (player_id < 1 || player_id > MAX_LOCAL_PLAYERS) {
        return 0;
    }
    LocalPlayer *local = &local_players[player_id - 1];
    int hw = hit_test_face(local->player, x, y, z, face);
    if (hw > 0) {
        return 1;
    }
    return 0;
}

void reset_model(void)
{
    chunks_reset();
    clients_reset();
    local_players_init();
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        local_player_reset(&local_players[i]);
    }
    config->day_length = DAY_LENGTH;
    const unsigned char *stored_time;
    stored_time = db_get_option("time");
    if (config->time >= 0 && config->time <= 24) {
        pg_set_time(config->day_length /
                    (24.0 / (config->time == 0 ? 24 : config->time)));
    } else if (stored_time != NULL) {
        pg_set_time(config->day_length * atof((char *)stored_time));
    } else {
        pg_set_time(config->day_length / 3.0);
    }
    g->time_changed = 1;
    set_player_count(clients, config->players);
    ring_empty(&g->edit_ring);
    ring_alloc(&g->edit_ring, 1024);
}

int render_3D_scene(LocalPlayer *local, Player* player, float ts)
{
    State *s = &player->state;
    int face_count = 0;
    render_sky();
    glClear(GL_DEPTH_BUFFER_BIT);
    ensure_chunks(player);
    face_count = render_chunks();
    render_signs();
    if (local->typing && local->typing_buffer[0] == CRAFT_KEY_SIGN) {
        int x, y, z, face;
        if (hit_test_face(player, &x, &y, &z, &face)) {
            int shape = get_shape(x, y, z);
            float y_face_height = item_height(shape);
            render_sign(local->typing_buffer, x, y, z, face, y_face_height);
        }
    }
    render_players(player);
    if (config->show_wireframe) {
        int hx, hy, hz;
        int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
        if (is_obstacle(hw, 0, 0)) {
            const float *color;
            if (is_control(get_extra(hx, hy, hz))) {
                color = RED;
            } else {
                color = BLACK;
            }
            int shape = get_shape(hx, hy, hz);
            render_wireframe(hx, hy, hz, color, item_height(shape));
        }
    }
    if (config->show_player_names) {
        Player *other = player_crosshair(player);
        if (other) {
            render_text(ALIGN_CENTER,
                g->width / 2, g->height / 2 - ts - 24, ts,
                other->name);
        }
    }
    return face_count;
}

void render_HUD(LocalPlayer *local, Player* player)
{
    State *s = &player->state;
    glClear(GL_DEPTH_BUFFER_BIT);
    if (config->show_crosshairs && local->active_menu == NULL) {
        int hx, hy, hz;
        int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
        const float *color;
        if (is_obstacle(hw, 0, 0) && is_control(get_extra(hx, hy, hz))) {
            color = RED;
        } else {
            color = BLACK;
        }
        render_crosshairs(color);
    }
    if (config->show_item) {
        render_item(items[local->item_index]);
    }
}

void render_picture_in_picture(LocalPlayer *local, Player* player, float ts)
{
    if (local->observe2 && find_client(local->observe2_client_id)) {
        player = (find_client(local->observe2_client_id))->players +
                 (local->observe2 - 1);

        int pw = local->view_width / 4 * g->scale;
        int ph = local->view_height / 3 * g->scale;
        int offset = 32 * g->scale;
        int pad = 3 * g->scale;
        int sw = pw + pad * 2;
        int sh = ph + pad * 2;

        glEnable(GL_SCISSOR_TEST);
        glScissor(g->width - sw - offset + pad + local->view_x,
                  offset - pad + local->view_y, sw, sh);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
        glViewport(g->width - pw - offset + local->view_x,
                   offset + local->view_y, pw, ph);

        g->width = pw;
        g->height = ph;
        g->ortho = 0;
        g->fov = 65;
        render_set_state(&player->state, pw, ph, g->render_radius,
            g->sign_radius, g->ortho, g->fov, g->scale, g->gl_float_type,
            g->float_size);

        render_sky();
        glClear(GL_DEPTH_BUFFER_BIT);
        ensure_chunks(player);
        render_chunks();
        render_signs();
        render_players(player);
        glClear(GL_DEPTH_BUFFER_BIT);
        if (config->show_player_names) {
            render_text(ALIGN_CENTER,
                pw / 2, ts, ts, player->name);
        }
    }
}

void render_HUD_text(LocalPlayer *local, Player* player, float ts, FPS fps, int face_count)
{
    State *s = &player->state;
    char text_buffer[1024];
    float tx = ts / 2;
    float ty = g->height - ts;
    if (config->show_info_text) {
        int hour = time_of_day() * 24;
        char am_pm = hour < 12 ? 'a' : 'p';
        hour = hour % 12;
        hour = hour ? hour : 12;
        if (config->verbose) {
            snprintf(
                text_buffer, 1024,
                "(%d, %d) (%.2f, %.2f, %.2f) [%d, %d, %d] %d%cm",
                chunked(s->x), chunked(s->z), s->x, s->y, s->z,
                client_count, chunk_count,
                face_count * 2, hour, am_pm);
            render_text(ALIGN_LEFT, tx, ty, ts, text_buffer);
            ty -= ts * 2;

            // FPS counter in lower right corner
            float bottom_bar_y = 0 + ts * 2;
            float right_side = g->width - ts;
            snprintf(text_buffer, 1024, "%dfps", fps.fps);
            render_text(ALIGN_RIGHT, right_side, bottom_bar_y, ts, text_buffer);
        } else {
            snprintf(text_buffer, 1024, "(%.2f, %.2f, %.2f)",
                     s->x, s->y, s->z);
            render_text(ALIGN_LEFT, tx, ty, ts, text_buffer);

            // Game time in upper right corner
            float right_side = g->width - tx;
            snprintf(text_buffer, 1024, "%d%cm", hour, am_pm);
            render_text(ALIGN_RIGHT, right_side, ty, ts, text_buffer);

            ty -= ts * 2;
        }
    }
    if (config->show_chat_text) {
        for (int i = 0; i < MAX_MESSAGES; i++) {
            int index = (local->message_index + i) % MAX_MESSAGES;
            if (strlen(local->messages[index])) {
                render_text(ALIGN_LEFT, tx, ty, ts, local->messages[index]);
                ty -= ts * 2;
            }
        }
    }
    if (local->typing == CommandLineFocus) {
        snprintf(text_buffer, 1024, "> %s", local->typing_buffer);
        render_text(ALIGN_LEFT, tx, ty, ts, text_buffer);
        glClear(GL_DEPTH_BUFFER_BIT);
        render_text_cursor(tx + ts * (local->text_cursor+1) + ts/2, ty);
        ty -= ts * 2;
    }
    if (config->players > 1) {
        // Render player name if more than 1 local player
        snprintf(text_buffer, 1024, "%s", player->name);
        render_text(ALIGN_CENTER, g->width/2, ts, ts, text_buffer);
    }
}

void render_player_world(LocalPlayer *local, FPS fps)
{
    Player *player = local->player;
    State *s = &player->state;
    float ts = 8 * g->scale;
    int face_count = 0;

    glViewport(local->view_x, local->view_y, local->view_width,
               local->view_height);
    g->width = local->view_width;
    g->height = local->view_height;
    g->ortho = local->ortho_is_pressed ? 64 : 0;
    g->fov = local->zoom_is_pressed ? 15 : 65;
    render_set_state(s, local->view_width, local->view_height, g->render_radius,
        g->sign_radius, g->ortho, g->fov, g->scale, g->gl_float_type,
        g->float_size);

    if (local->observe1 > 0 && find_client(local->observe1_client_id)) {
        player = find_client(local->observe1_client_id)->players +
                 (local->observe1 - 1);
    }

    if (local->show_world == 1) {
        face_count = render_3D_scene(local, player, ts);
        if (local->vt_open == 0) {
            render_HUD(local, player);
        }
        render_picture_in_picture(local, player, ts);
    }
    if (local->vt_open == 0) {
        render_HUD_text(local, player, ts, fps, face_count);
    }

    // RENDER VIRTUAL TERMINAL //
    if (local->pwt) {
        int r = vt_process(local->pwt);
        if (r == false) {
            destroy_vt(local);
        }
        if (local->vt_open) {
            float x = 0;
            float y = local->view_height;
            glClear(GL_DEPTH_BUFFER_BIT);
            vt_draw(local->pwt, x, y, local->vt_scale);
        }
    }

    if (local->active_menu) {
        glClear(GL_DEPTH_BUFFER_BIT);
        menu_render(local->active_menu, g->width, g->height, g->scale);
        glClear(GL_DEPTH_BUFFER_BIT);
        render_mouse_cursor(local->mouse_x, local->mouse_y,
                            local->player->id);
    }
}

/*
 * Set view radius that will fit into the current size of GPU RAM.
 */
void set_view_radius(int requested_size, int delete_request)
{
    int radius = requested_size;
    int delete_radius = delete_request;
    if (!config->no_limiters) {
        int gpu_mb = pg_get_gpu_mem_size();
        if (gpu_mb < 48 || (gpu_mb < 64 && config->players >= 2) ||
            (gpu_mb < 128 && config->players >= 4)) {
            // A draw distance of 1 is not enough for the game to be usable,
            // but this does at least show something on screen (for low
            // resolutions only - higher ones will crash the game with low GPU
            // RAM).
            radius = 1;
            delete_radius = radius + 1;
        } else if (gpu_mb < 64 || (gpu_mb < 128 && config->players >= 3)) {
            radius = 2;
            delete_radius = radius + 1;
        } else if (gpu_mb < 128 && config->players >= 2) {
            radius = 2;
            delete_radius = radius + 2;
        } else if (gpu_mb < 128 || (gpu_mb < 256 && config->players >= 3)) {
            // A GPU RAM size of 64M will result in rendering issues for draw
            // distances greater than 3 (with a chunk size of 16).
            radius = 3;
            delete_radius = radius + 2;
        } else if (gpu_mb < 256 || requested_size == AUTO_PICK_RADIUS) {
            // For the Raspberry Pi reduce amount to draw to both fit into
            // 128MiB of GPU RAM and keep the render speed at a reasonable
            // smoothness.
            radius = 5;
            delete_radius = radius + 3;
        }
    }

    if (radius <= 0) {
        radius = 5;
    }

    if (delete_radius < radius) {
        delete_radius = radius + 3;
    }

    g->create_radius = radius;
    g->render_radius = radius;
    g->delete_radius = delete_radius;
    g->sign_radius = radius;

    if (config->verbose) {
        printf("\nradii: create: %d render: %d delete: %d sign: %d\n",
               g->create_radius, g->render_radius, g->delete_radius,
               g->sign_radius);
    }
}

// call after changes to local player count
void recheck_view_radius(void)
{
    set_view_radius(g->render_radius, g->delete_radius);
}

void set_render_radius(int radius)
{
    set_view_radius(radius, g->delete_radius);
}

void set_delete_radius(int radius)
{
    if (radius >= g->create_radius && radius <= g->create_radius + 30) {
        set_view_radius(g->render_radius, radius);
    }
}

void recheck_players_view_size(void)
{
    set_players_view_size(g->width, g->height);
}

void pw_get_player_pos(int player_id, float *x, float *y, float *z)
{
    if (player_id < 1 || player_id > MAX_LOCAL_PLAYERS) {
        return;
    }
    State *s = &local_players[player_id - 1].player->state;
    *x = s->x;
    *y = s->y;
    *z = s->z;
}

void pw_set_player_pos(int player_id, float x, float y, float z)
{
    if (player_id < 1 || player_id > MAX_LOCAL_PLAYERS) {
        return;
    }
    State *s = &local_players[player_id - 1].player->state;
    s->x = x;
    s->y = y;
    s->z = z;
}

void pw_get_player_angle(int player_id, float *x, float *y)
{
    if (player_id < 1 || player_id > MAX_LOCAL_PLAYERS) {
        return;
    }
    State *s = &local_players[player_id - 1].player->state;
    *x = s->rx;
    *y = s->ry;
}

void pw_set_player_angle(int player_id, float x, float y)
{
    if (player_id < 1 || player_id > MAX_LOCAL_PLAYERS) {
        return;
    }
    State *s = &local_players[player_id - 1].player->state;
    s->rx = x;
    s->ry = y;
}

int pw_get_time(void)
{
    return time_of_day() * 24;
}

void pw_set_time(int time)
{
    pg_set_time(config->day_length / (24.0 / (time == 0 ? 24 : time)));
    g->time_changed = 1;
}

void set_time_elapsed_and_day_length(float elapsed, int day_length)
{
    pg_set_time(fmod(elapsed, day_length));
    config->day_length = day_length;
    g->time_changed = 1;
}

void drain_edit_queue(size_t max_items, double max_time, double now)
{
    if (!ring_empty(&g->edit_ring) && mtx_trylock(&edit_ring_mtx)) {
        RingEntry e;
        for (size_t i=0; i<max_items; i++) {
            int edit_ready = ring_get(&g->edit_ring, &e);
            if (edit_ready) {
                switch (e.type) {
                case BLOCK:
                    set_block(e.x, e.y, e.z, e.w);
                    break;
                case EXTRA:
                    set_extra(e.x, e.y, e.z, e.w);
                    break;
                case SHAPE:
                    set_shape(e.x, e.y, e.z, e.w);
                    break;
                case LIGHT:
                    set_light(chunked(e.x), chunked(e.z), e.x, e.y, e.z, e.w);
                    break;
                case SIGN:
                    set_sign(e.x, e.y, e.z, e.w, e.sign);
                    free(e.sign);
                    break;
                case TRANSFORM:
                    set_transform(e.x, e.y, e.z, e.w);
                    break;
                case KEY:
                case COMMIT:
                case EXIT:
                default:
                    printf("Edit ring does not support: %d\n", e.type);
                    break;
                }
            } else {
                break;
            }
            if (pg_get_time() - now > max_time) {
                break;
            }
        }
        mtx_unlock(&edit_ring_mtx);
    }
}

void initialize_worker_threads(void)
{
    for (int i = 0; i < config->worker_count; i++) {
        Worker *worker = g->workers + i;
        worker->index = i;
        worker->state = WORKER_IDLE;
        worker->exit_requested = False;
        mtx_init(&worker->mtx, mtx_plain);
        cnd_init(&worker->cnd);
        thrd_create(&worker->thrd, worker_run, worker);
    }
}

void deinitialize_worker_threads(void)
{
    // Stop thread processing
    for (int i = 0; i < config->worker_count; i++) {
        Worker *worker = g->workers + i;
        worker->exit_requested = True;
        cnd_signal(&worker->cnd);
    }
    // Wait for worker threads to exit
    for (int i = 0; i < config->worker_count; i++) {
        Worker *worker = g->workers + i;
        thrd_join(worker->thrd, NULL);
        cnd_destroy(&worker->cnd);
        mtx_destroy(&worker->mtx);
    }
}

void pw_new_game(char *path)
{
    // Create a new game
    g->mode_changed = 1;
    g->mode = MODE_OFFLINE;
    snprintf(g->db_path, MAX_PATH_LENGTH, "%s", path);
}

void pw_load_game(char *path)
{
    g->mode_changed = 1;
    g->mode = MODE_OFFLINE;
    snprintf(g->db_path, MAX_PATH_LENGTH, "%s", path);
}

void pw_connect_to_server(char *server_addr, int server_port)
{
    g->mode_changed = 1;
    g->mode = MODE_ONLINE;
    strncpy(config->server, server_addr, MAX_ADDR_LENGTH);
    config->port = server_port;
    get_server_db_cache_path(g->db_path);
}

int is_online(void)
{
    return g->mode == MODE_ONLINE;
}

int is_offline(void)
{
    return g->mode == MODE_OFFLINE;
}

void set_show_clouds(int option)
{
    char value[2];
    snprintf(value, 2, "%d", option);
    db_set_option("show-clouds", value);
    config->show_clouds = option;
    g->render_option_changed = 1;  // regenerate world
}

void set_show_lights(int option)
{
    config->show_lights = option;
    g->render_option_changed = 1;  // regenerate world
}

void set_show_plants(int option)
{
    char value[2];
    snprintf(value, 2, "%d", option);
    db_set_option("show-plants", value);
    config->show_plants = option;
    g->render_option_changed = 1;  // regenerate world
}

void set_show_trees(int option)
{
    char value[2];
    snprintf(value, 2, "%d", option);
    db_set_option("show-trees", value);
    config->show_trees = option;
    g->render_option_changed = 1;  // regenerate world
}

