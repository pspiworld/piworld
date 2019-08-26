
#include <assert.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"
#include "config.h"
#include "cube.h"
#include "db.h"
#include "item.h"
#include "map.h"
#include "matrix.h"
#include "noise.h"
#include "pg.h"
#include "pg_joystick.h"
#include "sign.h"
#include "tinycthread.h"
#include "util.h"
#include "world.h"
#include "x11_event_handler.h"

#define MAX_CHUNKS 8192
#define MAX_CLIENTS 128
#define MAX_LOCAL_PLAYERS 4
#define WORKERS 1
#define MAX_TEXT_LENGTH 256
#define MAX_NAME_LENGTH 32

#define MAX_HISTORY_SIZE 20
#define CHAT_HISTORY 0
#define COMMAND_HISTORY 1
#define SIGN_HISTORY 2
#define NUM_HISTORIES 3
#define NOT_IN_HISTORY -1

#define ALIGN_LEFT 0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT 2

#define MODE_OFFLINE 0
#define MODE_ONLINE 1

#define WORKER_IDLE 0
#define WORKER_BUSY 1
#define WORKER_DONE 2

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

const float GREEN[4] = {0.0, 1.0, 0.0, 1.0};
const float BLACK[4] = {0.0, 0.0, 0.0, 1.0};

static int terminate;

typedef struct {
    Map map;
    Map lights;
    SignList signs;
    int p;
    int q;
    int faces;
    int sign_faces;
    int dirty;
    int miny;
    int maxy;
    GLuint buffer;
    GLuint sign_buffer;
} Chunk;

typedef struct {
    int p;
    int q;
    int load;
    Map *block_maps[3][3];
    Map *light_maps[3][3];
    int miny;
    int maxy;
    int faces;
    GLfloat *data;
} WorkerItem;

typedef struct {
    int index;
    int state;
    thrd_t thrd;
    mtx_t mtx;
    cnd_t cnd;
    WorkerItem item;
    int exit_requested;
} Worker;

typedef struct {
    int x;
    int y;
    int z;
    int w;
} Block;

typedef struct {
    float x;
    float y;
    float z;
    float rx;
    float ry;
    float t;
} State;

typedef struct Player {
    char name[MAX_NAME_LENGTH];
    int id;  // 1...MAX_LOCAL_PLAYERS
    State state;
    State state1;
    State state2;
    GLuint buffer;
    int texture_index;
    int is_active;
} Player;

typedef struct {
    int id;
    Player players[MAX_LOCAL_PLAYERS];
} Client;

typedef struct {
    Player *player;
    int item_index;
    int flying;
    float dy;
    int typing;

    int view_x;
    int view_y;
    int view_width;
    int view_height;

    int forward_is_pressed;
    int back_is_pressed;
    int left_is_pressed;
    int right_is_pressed;
    int jump_is_pressed;
    int crouch_is_pressed;
    int view_left_is_pressed;
    int view_right_is_pressed;
    int view_up_is_pressed;
    int view_down_is_pressed;
    int ortho_is_pressed;
    int zoom_is_pressed;
    float view_speed_left_right;
    float view_speed_up_down;
    float movement_speed_left_right;
    float movement_speed_forward_back;
    int shoulder_button_mode;

    Block block0;
    Block block1;
    Block copy0;
    Block copy1;

    int observe1;
    int observe1_client_id;
    int observe2;
    int observe2_client_id;
} LocalPlayer;

typedef struct {
    GLuint program;
    GLuint position;
    GLuint normal;
    GLuint uv;
    GLuint matrix;
    GLuint sampler;
    GLuint camera;
    GLuint timer;
    GLuint extra1;
    GLuint extra2;
    GLuint extra3;
    GLuint extra4;
} Attrib;

typedef struct {
    char lines[MAX_HISTORY_SIZE][MAX_TEXT_LENGTH];
    int size;
    int end;
    int line_start;
} TextLineHistory;

typedef struct {
    Worker workers[WORKERS];
    Chunk chunks[MAX_CHUNKS];
    int chunk_count;
    int create_radius;
    int render_radius;
    int delete_radius;
    int sign_radius;
    Client clients[MAX_CLIENTS];
    LocalPlayer local_players[MAX_LOCAL_PLAYERS];
    LocalPlayer *keyboard_player;
    LocalPlayer *mouse_player;
    int client_count;
    char typing_buffer[MAX_TEXT_LENGTH];
    TextLineHistory typing_history[NUM_HISTORIES];
    int history_position;
    int text_cursor;
    int typing_start;
    int message_index;
    char messages[MAX_MESSAGES][MAX_TEXT_LENGTH];
    int width;
    int height;
    float scale;
    int ortho;
    float fov;
    int suppress_char;
    int mode;
    int mode_changed;
    char db_path[MAX_PATH_LENGTH];
    int day_length;
    int time_changed;
    int auto_match_players_to_joysticks;
} Model;

static Model model;
static Model *g = &model;

void move_primary_focus_to_active_player(void);
void set_players_view_size(int w, int h);
Client *find_client(int id);
void set_view_radius(int requested_size);

void limit_player_count_to_fit_gpu_mem()
{
    if (pg_get_gpu_mem_size() < 128 && config->players > 2) {
        printf("More GPU memory needed for more players.\n");
        config->players = 2;
    }
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

int get_first_active_player(Client *client) {
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

int get_next_player(int *player_index, int *client_id)
{
    Client *client = find_client(*client_id);
    if (!client) {
        client = g->clients;
        *client_id = client->id;
        *player_index = 1;
    }
    int next = get_next_local_player(client, *player_index);
    if (next <= *player_index) {
        *client_id = (*client_id + 1) % (g->client_count + 1);
        if (*client_id == 0) {
            *client_id = 1;
        }
        client = find_client(*client_id);
        if (!client) {
            client = g->clients;
            *client_id = client->id;
        }
        next = get_first_active_player(client);
    }
    *player_index = next;
}

int chunked(float x) {
    return floorf(roundf(x) / CHUNK_SIZE);
}

float time_of_day() {
    if (g->day_length <= 0) {
        return 0.5;
    }
    float t;
    t = pg_get_time();
    t = t / g->day_length;
    t = t - (int)t;
    return t;
}

float get_daylight() {
    float timer = time_of_day();
    if (timer < 0.5) {
        float t = (timer - 0.25) * 100;
        return 1 / (1 + powf(2, -t));
    }
    else {
        float t = (timer - 0.85) * 100;
        return 1 - 1 / (1 + powf(2, -t));
    }
}

float get_scale_factor() {
    return 1.0;
}

void get_sight_vector(float rx, float ry, float *vx, float *vy, float *vz) {
    float m = cosf(ry);
    *vx = cosf(rx - RADIANS(90)) * m;
    *vy = sinf(ry);
    *vz = sinf(rx - RADIANS(90)) * m;
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

GLuint gen_crosshair_buffer() {
    int x = g->width / 2;
    int y = g->height / 2;
    int p = 10 * g->scale;
    float data[] = {
        x, y - p, x, y + p,
        x - p, y, x + p, y
    };
    return gen_buffer(sizeof(data), data);
}

GLuint gen_wireframe_buffer(float x, float y, float z, float n) {
    float data[72];
    make_cube_wireframe(data, x, y, z, n);
    return gen_buffer(sizeof(data), data);
}

GLuint gen_sky_buffer() {
    float data[12288];
    make_sphere(data, 1, 3);
    return gen_buffer(sizeof(data), data);
}

GLuint gen_cube_buffer(float x, float y, float z, float n, int w) {
    GLfloat *data = malloc_faces(10, 6);
    float ao[6][4] = {0};
    float light[6][4] = {
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5}
    };
    make_cube(data, ao, light, 1, 1, 1, 1, 1, 1, x, y, z, n, w);
    return gen_faces(10, 6, data);
}

GLuint gen_plant_buffer(float x, float y, float z, float n, int w) {
    GLfloat *data = malloc_faces(10, 4);
    float ao = 0;
    float light = 1;
    make_plant(data, ao, light, x, y, z, n, w, 45);
    return gen_faces(10, 4, data);
}

GLuint gen_player_buffer(float x, float y, float z, float rx, float ry, int p) {
    GLfloat *data = malloc_faces(10, 6);
    make_player(data, x, y, z, rx, ry, p);
    return gen_faces(10, 6, data);
}

GLuint gen_text_buffer(float x, float y, float n, char *text) {
    int length = strlen(text);
    GLfloat *data = malloc_faces(4, length);
    for (int i = 0; i < length; i++) {
        make_character(data + i * 24, x, y, n / 2, n, text[i]);
        x += n;
    }
    return gen_faces(4, length, data);
}

void draw_triangles_3d_ao(Attrib *attrib, GLuint buffer, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->normal);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 3, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 10, 0);
    glVertexAttribPointer(attrib->normal, 3, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 10, (GLvoid *)(sizeof(GLfloat) * 3));
    glVertexAttribPointer(attrib->uv, 4, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 10, (GLvoid *)(sizeof(GLfloat) * 6));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->normal);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_triangles_3d_text(Attrib *attrib, GLuint buffer, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 3, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 5, 0);
    glVertexAttribPointer(attrib->uv, 2, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 5, (GLvoid *)(sizeof(GLfloat) * 3));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_triangles_3d_sky(Attrib *attrib, GLuint buffer, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 3, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 8, 0);
    glVertexAttribPointer(attrib->uv, 2, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 8, (GLvoid *)(sizeof(GLfloat) * 6));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_triangles_2d(Attrib *attrib, GLuint buffer, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 2, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 4, 0);
    glVertexAttribPointer(attrib->uv, 2, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 4, (GLvoid *)(sizeof(GLfloat) * 2));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_lines(Attrib *attrib, GLuint buffer, int components, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glVertexAttribPointer(
        attrib->position, components, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_LINES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_chunk(Attrib *attrib, Chunk *chunk) {
    draw_triangles_3d_ao(attrib, chunk->buffer, chunk->faces * 6);
}

void draw_item(Attrib *attrib, GLuint buffer, int count) {
    draw_triangles_3d_ao(attrib, buffer, count);
}

void draw_text(Attrib *attrib, GLuint buffer, int length) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_triangles_2d(attrib, buffer, length * 6);
    glDisable(GL_BLEND);
}

void draw_signs(Attrib *attrib, Chunk *chunk) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-8, -1024);
    draw_triangles_3d_text(attrib, chunk->sign_buffer, chunk->sign_faces * 6);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void draw_sign(Attrib *attrib, GLuint buffer, int length) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-8, -1024);
    draw_triangles_3d_text(attrib, buffer, length * 6);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void draw_cube(Attrib *attrib, GLuint buffer) {
    draw_item(attrib, buffer, 36);
}

void draw_plant(Attrib *attrib, GLuint buffer) {
    draw_item(attrib, buffer, 24);
}

void draw_player(Attrib *attrib, Player *player) {
    draw_cube(attrib, player->buffer);
}

Client *find_client(int id) {
    for (int i = 0; i < g->client_count; i++) {
        Client *client = g->clients + i;
        if (client->id == id) {
            return client;
        }
    }
    return 0;
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

void interpolate_player(Player *player) {
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

void delete_client(int id) {
    Client *client = find_client(id);
    if (!client) {
        return;
    }
    int count = g->client_count;
    for (int i = 0; i<MAX_LOCAL_PLAYERS; i++) {
        Player *player = client->players + i;
        if (player->is_active) {
            del_buffer(player->buffer);
        }
    }
    Client *other = g->clients + (--count);
    memcpy(client, other, sizeof(Client));
    g->client_count = count;
}

void delete_all_players() {
    for (int i = 0; i < g->client_count; i++) {
        Client *client = g->clients + i;
        for (int j = 0; j < MAX_LOCAL_PLAYERS; j++) {
            Player *player = client->players + j;
            if (player->is_active) {
                del_buffer(player->buffer);
            }
        }
    }
    g->client_count = 0;
}

float player_player_distance(Player *p1, Player *p2) {
    State *s1 = &p1->state;
    State *s2 = &p2->state;
    float x = s2->x - s1->x;
    float y = s2->y - s1->y;
    float z = s2->z - s1->z;
    return sqrtf(x * x + y * y + z * z);
}

float player_crosshair_distance(Player *p1, Player *p2) {
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

Player *player_crosshair(Player *player) {
    Player *result = 0;
    float threshold = RADIANS(5);
    float best = 0;
    for (int i = 0; i < g->client_count; i++) {
        Client *client = g->clients + i;
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

Chunk *find_chunk(int p, int q) {
    for (int i = 0; i < g->chunk_count; i++) {
        Chunk *chunk = g->chunks + i;
        if (chunk->p == p && chunk->q == q) {
            return chunk;
        }
    }
    return 0;
}

int chunk_distance(Chunk *chunk, int p, int q) {
    int dp = ABS(chunk->p - p);
    int dq = ABS(chunk->q - q);
    return MAX(dp, dq);
}

int chunk_visible(float planes[6][4], int p, int q, int miny, int maxy) {
    int x = p * CHUNK_SIZE - 1;
    int z = q * CHUNK_SIZE - 1;
    int d = CHUNK_SIZE + 1;
    float points[8][3] = {
        {x + 0, miny, z + 0},
        {x + d, miny, z + 0},
        {x + 0, miny, z + d},
        {x + d, miny, z + d},
        {x + 0, maxy, z + 0},
        {x + d, maxy, z + 0},
        {x + 0, maxy, z + d},
        {x + d, maxy, z + d}
    };
    int n = g->ortho ? 4 : 6;
    for (int i = 0; i < n; i++) {
        int in = 0;
        int out = 0;
        for (int j = 0; j < 8; j++) {
            float d =
                planes[i][0] * points[j][0] +
                planes[i][1] * points[j][1] +
                planes[i][2] * points[j][2] +
                planes[i][3];
            if (d < 0) {
                out++;
            }
            else {
                in++;
            }
            if (in && out) {
                break;
            }
        }
        if (in == 0) {
            return 0;
        }
    }
    return 1;
}

int highest_block(float x, float z) {
    int result = -1;
    int nx = roundf(x);
    int nz = roundf(z);
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->map;
        MAP_FOR_EACH(map, ex, ey, ez, ew) {
            if (is_obstacle(ew) && ex == nx && ez == nz) {
                result = MAX(result, ey);
            }
        } END_MAP_FOR_EACH;
    }
    return result;
}

int _hit_test(
    Map *map, float max_distance, int previous,
    float x, float y, float z,
    float vx, float vy, float vz,
    int *hx, int *hy, int *hz)
{
    int m = 32;
    int px = 0;
    int py = 0;
    int pz = 0;
    for (int i = 0; i < max_distance * m; i++) {
        int nx = roundf(x);
        int ny = roundf(y);
        int nz = roundf(z);
        if (nx != px || ny != py || nz != pz) {
            int hw = map_get(map, nx, ny, nz);
            if (hw > 0) {
                if (previous) {
                    *hx = px; *hy = py; *hz = pz;
                }
                else {
                    *hx = nx; *hy = ny; *hz = nz;
                }
                return hw;
            }
            px = nx; py = ny; pz = nz;
        }
        x += vx / m; y += vy / m; z += vz / m;
    }
    return 0;
}

int hit_test(
    int previous, float x, float y, float z, float rx, float ry,
    int *bx, int *by, int *bz)
{
    int result = 0;
    float best = 0;
    int p = chunked(x);
    int q = chunked(z);
    float vx, vy, vz;
    get_sight_vector(rx, ry, &vx, &vy, &vz);
    for (int i = 0; i < g->chunk_count; i++) {
        Chunk *chunk = g->chunks + i;
        if (chunk_distance(chunk, p, q) > 1) {
            continue;
        }
        int hx, hy, hz;
        int hw = _hit_test(&chunk->map, 8, previous,
            x, y, z, vx, vy, vz, &hx, &hy, &hz);
        if (hw > 0) {
            float d = sqrtf(
                powf(hx - x, 2) + powf(hy - y, 2) + powf(hz - z, 2));
            if (best == 0 || d < best) {
                best = d;
                *bx = hx; *by = hy; *bz = hz;
                result = hw;
            }
        }
    }
    return result;
}

int hit_test_face(Player *player, int *x, int *y, int *z, int *face) {
    State *s = &player->state;
    int w = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, x, y, z);
    if (is_obstacle(w)) {
        int hx, hy, hz;
        hit_test(1, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
        int dx = hx - *x;
        int dy = hy - *y;
        int dz = hz - *z;
        if (dx == -1 && dy == 0 && dz == 0) {
            *face = 0; return 1;
        }
        if (dx == 1 && dy == 0 && dz == 0) {
            *face = 1; return 1;
        }
        if (dx == 0 && dy == 0 && dz == -1) {
            *face = 2; return 1;
        }
        if (dx == 0 && dy == 0 && dz == 1) {
            *face = 3; return 1;
        }
        if (dx == 0 && dy == 1 && dz == 0) {
            int degrees = roundf(DEGREES(atan2f(s->x - hx, s->z - hz)));
            if (degrees < 0) {
                degrees += 360;
            }
            int top = ((degrees + 45) / 90) % 4;
            *face = 4 + top; return 1;
        }
    }
    return 0;
}

int collide(int height, float *x, float *y, float *z) {
    int result = 0;
    int p = chunked(*x);
    int q = chunked(*z);
    Chunk *chunk = find_chunk(p, q);
    if (!chunk) {
        return result;
    }
    Map *map = &chunk->map;
    int nx = roundf(*x);
    int ny = roundf(*y);
    int nz = roundf(*z);
    float px = *x - nx;
    float py = *y - ny;
    float pz = *z - nz;
    float pad = 0.25;
    for (int dy = 0; dy < height; dy++) {
        if (px < -pad && is_obstacle(map_get(map, nx - 1, ny - dy, nz))) {
            *x = nx - pad;
        }
        if (px > pad && is_obstacle(map_get(map, nx + 1, ny - dy, nz))) {
            *x = nx + pad;
        }
        if (py < -pad && is_obstacle(map_get(map, nx, ny - dy - 1, nz))) {
            *y = ny - pad;
            result = 1;
        }
        if (py > pad && is_obstacle(map_get(map, nx, ny - dy + 1, nz))) {
            *y = ny + pad;
            result = 1;
        }
        if (pz < -pad && is_obstacle(map_get(map, nx, ny - dy, nz - 1))) {
            *z = nz - pad;
        }
        if (pz > pad && is_obstacle(map_get(map, nx, ny - dy, nz + 1))) {
            *z = nz + pad;
        }
    }
    return result;
}

int player_intersects_block(
    int height,
    float x, float y, float z,
    int hx, int hy, int hz)
{
    int nx = roundf(x);
    int ny = roundf(y);
    int nz = roundf(z);
    for (int i = 0; i < height; i++) {
        if (nx == hx && ny - i == hy && nz == hz) {
            return 1;
        }
    }
    return 0;
}

int _gen_sign_buffer(
    GLfloat *data, float x, float y, float z, int face, const char *text)
{
    static const int glyph_dx[8] = {0, 0, -1, 1, 1, 0, -1, 0};
    static const int glyph_dz[8] = {1, -1, 0, 0, 0, -1, 0, 1};
    static const int line_dx[8] = {0, 0, 0, 0, 0, 1, 0, -1};
    static const int line_dy[8] = {-1, -1, -1, -1, 0, 0, 0, 0};
    static const int line_dz[8] = {0, 0, 0, 0, 1, 0, -1, 0};
    if (face < 0 || face >= 8) {
        return 0;
    }
    int count = 0;
    float max_width = 64;
    float line_height = 1.25;
    char lines[1024];
    int rows = wrap(text, max_width, lines, 1024);
    rows = MIN(rows, 5);
    int dx = glyph_dx[face];
    int dz = glyph_dz[face];
    int ldx = line_dx[face];
    int ldy = line_dy[face];
    int ldz = line_dz[face];
    float n = 1.0 / (max_width / 10);
    float sx = x - n * (rows - 1) * (line_height / 2) * ldx;
    float sy = y - n * (rows - 1) * (line_height / 2) * ldy;
    float sz = z - n * (rows - 1) * (line_height / 2) * ldz;
    char *key;
    char *line = tokenize(lines, "\n", &key);
    while (line) {
        int length = strlen(line);
        int line_width = string_width(line);
        line_width = MIN(line_width, max_width);
        float rx = sx - dx * line_width / max_width / 2;
        float ry = sy;
        float rz = sz - dz * line_width / max_width / 2;
        for (int i = 0; i < length; i++) {
            int width = char_width(line[i]);
            line_width -= width;
            if (line_width < 0) {
                break;
            }
            rx += dx * width / max_width / 2;
            rz += dz * width / max_width / 2;
            if (line[i] != ' ') {
                make_character_3d(
                    data + count * 30, rx, ry, rz, n / 2, face, line[i]);
                count++;
            }
            rx += dx * width / max_width / 2;
            rz += dz * width / max_width / 2;
        }
        sx += n * line_height * ldx;
        sy += n * line_height * ldy;
        sz += n * line_height * ldz;
        line = tokenize(NULL, "\n", &key);
        rows--;
        if (rows <= 0) {
            break;
        }
    }
    return count;
}

void gen_sign_buffer(Chunk *chunk) {
    SignList *signs = &chunk->signs;

    // first pass - count characters
    int max_faces = 0;
    for (int i = 0; i < signs->size; i++) {
        Sign *e = signs->data + i;
        max_faces += strlen(e->text);
    }

    // second pass - generate geometry
    GLfloat *data = malloc_faces(5, max_faces);
    int faces = 0;
    for (int i = 0; i < signs->size; i++) {
        Sign *e = signs->data + i;
        faces += _gen_sign_buffer(
            data + faces * 30, e->x, e->y, e->z, e->face, e->text);
    }

    del_buffer(chunk->sign_buffer);
    chunk->sign_buffer = gen_faces(5, faces, data);
    chunk->sign_faces = faces;
}

int has_lights(Chunk *chunk) {
    if (!config->show_lights) {
        return 0;
    }
    for (int dp = -1; dp <= 1; dp++) {
        for (int dq = -1; dq <= 1; dq++) {
            Chunk *other = chunk;
            if (dp || dq) {
                other = find_chunk(chunk->p + dp, chunk->q + dq);
            }
            if (!other) {
                continue;
            }
            Map *map = &other->lights;
            if (map->size) {
                return 1;
            }
        }
    }
    return 0;
}

void dirty_chunk(Chunk *chunk) {
    chunk->dirty = 1;
    if (has_lights(chunk)) {
        for (int dp = -1; dp <= 1; dp++) {
            for (int dq = -1; dq <= 1; dq++) {
                Chunk *other = find_chunk(chunk->p + dp, chunk->q + dq);
                if (other) {
                    other->dirty = 1;
                }
            }
        }
    }
}

void occlusion(
    char neighbors[27], char lights[27], float shades[27],
    float ao[6][4], float light[6][4])
{
    static const int lookup3[6][4][3] = {
        {{0, 1, 3}, {2, 1, 5}, {6, 3, 7}, {8, 5, 7}},
        {{18, 19, 21}, {20, 19, 23}, {24, 21, 25}, {26, 23, 25}},
        {{6, 7, 15}, {8, 7, 17}, {24, 15, 25}, {26, 17, 25}},
        {{0, 1, 9}, {2, 1, 11}, {18, 9, 19}, {20, 11, 19}},
        {{0, 3, 9}, {6, 3, 15}, {18, 9, 21}, {24, 15, 21}},
        {{2, 5, 11}, {8, 5, 17}, {20, 11, 23}, {26, 17, 23}}
    };
   static const int lookup4[6][4][4] = {
        {{0, 1, 3, 4}, {1, 2, 4, 5}, {3, 4, 6, 7}, {4, 5, 7, 8}},
        {{18, 19, 21, 22}, {19, 20, 22, 23}, {21, 22, 24, 25}, {22, 23, 25, 26}},
        {{6, 7, 15, 16}, {7, 8, 16, 17}, {15, 16, 24, 25}, {16, 17, 25, 26}},
        {{0, 1, 9, 10}, {1, 2, 10, 11}, {9, 10, 18, 19}, {10, 11, 19, 20}},
        {{0, 3, 9, 12}, {3, 6, 12, 15}, {9, 12, 18, 21}, {12, 15, 21, 24}},
        {{2, 5, 11, 14}, {5, 8, 14, 17}, {11, 14, 20, 23}, {14, 17, 23, 26}}
    };
    static const float curve[4] = {0.0, 0.25, 0.5, 0.75};
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 4; j++) {
            int corner = neighbors[lookup3[i][j][0]];
            int side1 = neighbors[lookup3[i][j][1]];
            int side2 = neighbors[lookup3[i][j][2]];
            int value = side1 && side2 ? 3 : corner + side1 + side2;
            float shade_sum = 0;
            float light_sum = 0;
            int is_light = lights[13] == 15;
            for (int k = 0; k < 4; k++) {
                shade_sum += shades[lookup4[i][j][k]];
                light_sum += lights[lookup4[i][j][k]];
            }
            if (is_light) {
                light_sum = 15 * 4 * 10;
            }
            float total = curve[value] + shade_sum / 4.0;
            ao[i][j] = MIN(total, 1.0);
            light[i][j] = light_sum / 15.0 / 4.0;
        }
    }
}

#define XZ_SIZE (CHUNK_SIZE * 3 + 2)
#define XZ_LO (CHUNK_SIZE)
#define XZ_HI (CHUNK_SIZE * 2 + 1)
#define Y_SIZE 258
#define XYZ(x, y, z) ((y) * XZ_SIZE * XZ_SIZE + (x) * XZ_SIZE + (z))
#define XZ(x, z) ((x) * XZ_SIZE + (z))

void light_fill(
    char *opaque, char *light,
    int x, int y, int z, int w, int force)
{
    if (x + w < XZ_LO || z + w < XZ_LO) {
        return;
    }
    if (x - w > XZ_HI || z - w > XZ_HI) {
        return;
    }
    if (y < 0 || y >= Y_SIZE) {
        return;
    }
    if (light[XYZ(x, y, z)] >= w) {
        return;
    }
    if (!force && opaque[XYZ(x, y, z)]) {
        return;
    }
    light[XYZ(x, y, z)] = w--;
    light_fill(opaque, light, x - 1, y, z, w, 0);
    light_fill(opaque, light, x + 1, y, z, w, 0);
    light_fill(opaque, light, x, y - 1, z, w, 0);
    light_fill(opaque, light, x, y + 1, z, w, 0);
    light_fill(opaque, light, x, y, z - 1, w, 0);
    light_fill(opaque, light, x, y, z + 1, w, 0);
}

void compute_chunk(WorkerItem *item) {
    char *opaque = (char *)calloc(XZ_SIZE * XZ_SIZE * Y_SIZE, sizeof(char));
    char *light = (char *)calloc(XZ_SIZE * XZ_SIZE * Y_SIZE, sizeof(char));
    char *highest = (char *)calloc(XZ_SIZE * XZ_SIZE, sizeof(char));

    int ox = item->p * CHUNK_SIZE - CHUNK_SIZE - 1;
    int oy = -1;
    int oz = item->q * CHUNK_SIZE - CHUNK_SIZE - 1;

    // check for lights
    int has_light = 0;
    if (config->show_lights) {
        for (int a = 0; a < 3; a++) {
            for (int b = 0; b < 3; b++) {
                Map *map = item->light_maps[a][b];
                if (map && map->size) {
                    has_light = 1;
                }
            }
        }
    }

    // populate opaque array
    for (int a = 0; a < 3; a++) {
        for (int b = 0; b < 3; b++) {
            Map *map = item->block_maps[a][b];
            if (!map) {
                continue;
            }
            MAP_FOR_EACH(map, ex, ey, ez, ew) {
                int x = ex - ox;
                int y = ey - oy;
                int z = ez - oz;
                int w = ew;
                // TODO: this should be unnecessary
                if (x < 0 || y < 0 || z < 0) {
                    continue;
                }
                if (x >= XZ_SIZE || y >= Y_SIZE || z >= XZ_SIZE) {
                    continue;
                }
                // END TODO
                opaque[XYZ(x, y, z)] = !is_transparent(w);
                if (opaque[XYZ(x, y, z)]) {
                    highest[XZ(x, z)] = MAX(highest[XZ(x, z)], y);
                }
            } END_MAP_FOR_EACH;
        }
    }

    // flood fill light intensities
    if (has_light) {
        for (int a = 0; a < 3; a++) {
            for (int b = 0; b < 3; b++) {
                Map *map = item->light_maps[a][b];
                if (!map) {
                    continue;
                }
                MAP_FOR_EACH(map, ex, ey, ez, ew) {
                    int x = ex - ox;
                    int y = ey - oy;
                    int z = ez - oz;
                    light_fill(opaque, light, x, y, z, ew, 1);
                } END_MAP_FOR_EACH;
            }
        }
    }

    Map *map = item->block_maps[1][1];

    // count exposed faces
    int miny = 256;
    int maxy = 0;
    int faces = 0;
    MAP_FOR_EACH(map, ex, ey, ez, ew) {
        if (ew <= 0) {
            continue;
        }
        int x = ex - ox;
        int y = ey - oy;
        int z = ez - oz;
        int f1 = !opaque[XYZ(x - 1, y, z)];
        int f2 = !opaque[XYZ(x + 1, y, z)];
        int f3 = !opaque[XYZ(x, y + 1, z)];
        int f4 = !opaque[XYZ(x, y - 1, z)] && (ey > 0);
        int f5 = !opaque[XYZ(x, y, z - 1)];
        int f6 = !opaque[XYZ(x, y, z + 1)];
        int total = f1 + f2 + f3 + f4 + f5 + f6;
        if (total == 0) {
            continue;
        }
        if (is_plant(ew)) {
            total = 4;
        }
        miny = MIN(miny, ey);
        maxy = MAX(maxy, ey);
        faces += total;
    } END_MAP_FOR_EACH;

    // generate geometry
    GLfloat *data = malloc_faces(10, faces);
    int offset = 0;
    MAP_FOR_EACH(map, ex, ey, ez, ew) {
        if (ew <= 0) {
            continue;
        }
        int x = ex - ox;
        int y = ey - oy;
        int z = ez - oz;
        int f1 = !opaque[XYZ(x - 1, y, z)];
        int f2 = !opaque[XYZ(x + 1, y, z)];
        int f3 = !opaque[XYZ(x, y + 1, z)];
        int f4 = !opaque[XYZ(x, y - 1, z)] && (ey > 0);
        int f5 = !opaque[XYZ(x, y, z - 1)];
        int f6 = !opaque[XYZ(x, y, z + 1)];
        int total = f1 + f2 + f3 + f4 + f5 + f6;
        if (total == 0) {
            continue;
        }
        char neighbors[27] = {0};
        char lights[27] = {0};
        float shades[27] = {0};
        int index = 0;
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    neighbors[index] = opaque[XYZ(x + dx, y + dy, z + dz)];
                    lights[index] = light[XYZ(x + dx, y + dy, z + dz)];
                    shades[index] = 0;
                    if (y + dy <= highest[XZ(x + dx, z + dz)]) {
                        for (int oy = 0; oy < 8; oy++) {
                            if (opaque[XYZ(x + dx, y + dy + oy, z + dz)]) {
                                shades[index] = 1.0 - oy * 0.125;
                                break;
                            }
                        }
                    }
                    index++;
                }
            }
        }
        float ao[6][4];
        float light[6][4];
        occlusion(neighbors, lights, shades, ao, light);
        if (is_plant(ew)) {
            total = 4;
            float min_ao = 1;
            float max_light = 0;
            for (int a = 0; a < 6; a++) {
                for (int b = 0; b < 4; b++) {
                    min_ao = MIN(min_ao, ao[a][b]);
                    max_light = MAX(max_light, light[a][b]);
                }
            }
            float rotation = simplex2(ex, ez, 4, 0.5, 2) * 360;
            make_plant(
                data + offset, min_ao, max_light,
                ex, ey, ez, 0.5, ew, rotation);
        }
        else {
            make_cube(
                data + offset, ao, light,
                f1, f2, f3, f4, f5, f6,
                ex, ey, ez, 0.5, ew);
        }
        offset += total * 60;
    } END_MAP_FOR_EACH;

    free(opaque);
    free(light);
    free(highest);

    item->miny = miny;
    item->maxy = maxy;
    item->faces = faces;
    item->data = data;
}

void generate_chunk(Chunk *chunk, WorkerItem *item) {
    chunk->miny = item->miny;
    chunk->maxy = item->maxy;
    chunk->faces = item->faces;
    del_buffer(chunk->buffer);
    chunk->buffer = gen_faces(10, item->faces, item->data);
    gen_sign_buffer(chunk);
}

void gen_chunk_buffer(Chunk *chunk) {
    WorkerItem _item;
    WorkerItem *item = &_item;
    item->p = chunk->p;
    item->q = chunk->q;
    for (int dp = -1; dp <= 1; dp++) {
        for (int dq = -1; dq <= 1; dq++) {
            Chunk *other = chunk;
            if (dp || dq) {
                other = find_chunk(chunk->p + dp, chunk->q + dq);
            }
            if (other) {
                item->block_maps[dp + 1][dq + 1] = &other->map;
                item->light_maps[dp + 1][dq + 1] = &other->lights;
            }
            else {
                item->block_maps[dp + 1][dq + 1] = 0;
                item->light_maps[dp + 1][dq + 1] = 0;
            }
        }
    }
    compute_chunk(item);
    generate_chunk(chunk, item);
    chunk->dirty = 0;
}

void map_set_func(int x, int y, int z, int w, void *arg) {
    Map *map = (Map *)arg;
    map_set(map, x, y, z, w);
}

void load_chunk(WorkerItem *item) {
    int p = item->p;
    int q = item->q;
    Map *block_map = item->block_maps[1][1];
    Map *light_map = item->light_maps[1][1];
    create_world(p, q, map_set_func, block_map);
    db_load_blocks(block_map, p, q);
    db_load_lights(light_map, p, q);
}

void request_chunk(int p, int q) {
    int key = db_get_key(p, q);
    client_chunk(p, q, key);
}

void init_chunk(Chunk *chunk, int p, int q) {
    chunk->p = p;
    chunk->q = q;
    chunk->faces = 0;
    chunk->sign_faces = 0;
    chunk->buffer = 0;
    chunk->sign_buffer = 0;
    dirty_chunk(chunk);
    SignList *signs = &chunk->signs;
    sign_list_alloc(signs, 16);
    db_load_signs(signs, p, q);
    Map *block_map = &chunk->map;
    Map *light_map = &chunk->lights;
    int dx = p * CHUNK_SIZE - 1;
    int dy = 0;
    int dz = q * CHUNK_SIZE - 1;
    map_alloc(block_map, dx, dy, dz, 0x7fff);
    map_alloc(light_map, dx, dy, dz, 0xf);
}

void create_chunk(Chunk *chunk, int p, int q) {
    init_chunk(chunk, p, q);

    WorkerItem _item;
    WorkerItem *item = &_item;
    item->p = chunk->p;
    item->q = chunk->q;
    item->block_maps[1][1] = &chunk->map;
    item->light_maps[1][1] = &chunk->lights;
    load_chunk(item);

    request_chunk(p, q);
}

void delete_chunks() {
    int count = g->chunk_count;
    int states_count = 0;
    // Maximum states include the basic player view and 2 observe views.
    #define MAX_STATES (MAX_LOCAL_PLAYERS * 3)
    State *states[MAX_STATES];

    for (int p = 0; p < MAX_LOCAL_PLAYERS; p++) {
        LocalPlayer *local = g->local_players + p;
        if (local->player->is_active) {
            states[states_count++] = &local->player->state;
            if (local->observe1) {
                Client *client = find_client(local->observe1_client_id);
                if (client) {
                    Player *observe_player = client->players + local->observe1
                                             - 1;
                    states[states_count++] = &observe_player->state;
                }
            }
            if (local->observe2) {
                Client *client = find_client(local->observe2_client_id);
                if (client) {
                    Player *observe_player = client->players + local->observe2
                                             - 1;
                    states[states_count++] = &observe_player->state;
                }
            }
        }
    }

    for (int i = 0; i < count; i++) {
        Chunk *chunk = g->chunks + i;
        int delete = 1;
        for (int j = 0; j < states_count; j++) {
            State *s = states[j];
            int p = chunked(s->x);
            int q = chunked(s->z);
            if (chunk_distance(chunk, p, q) < g->delete_radius) {
                delete = 0;
                break;
            }
        }
        if (delete) {
            map_free(&chunk->map);
            map_free(&chunk->lights);
            sign_list_free(&chunk->signs);
            del_buffer(chunk->buffer);
            del_buffer(chunk->sign_buffer);
            Chunk *other = g->chunks + (--count);
            memcpy(chunk, other, sizeof(Chunk));
        }
    }
    g->chunk_count = count;
}

void delete_all_chunks() {
    for (int i = 0; i < g->chunk_count; i++) {
        Chunk *chunk = g->chunks + i;
        map_free(&chunk->map);
        map_free(&chunk->lights);
        sign_list_free(&chunk->signs);
        del_buffer(chunk->buffer);
        del_buffer(chunk->sign_buffer);
    }
    g->chunk_count = 0;
}

void check_workers() {
    for (int i = 0; i < WORKERS; i++) {
        Worker *worker = g->workers + i;
        mtx_lock(&worker->mtx);
        if (worker->state == WORKER_DONE) {
            WorkerItem *item = &worker->item;
            Chunk *chunk = find_chunk(item->p, item->q);
            if (chunk) {
                if (item->load) {
                    Map *block_map = item->block_maps[1][1];
                    Map *light_map = item->light_maps[1][1];
                    map_free(&chunk->map);
                    map_free(&chunk->lights);
                    map_copy(&chunk->map, block_map);
                    map_copy(&chunk->lights, light_map);
                    request_chunk(item->p, item->q);
                }
                generate_chunk(chunk, item);
            }
            for (int a = 0; a < 3; a++) {
                for (int b = 0; b < 3; b++) {
                    Map *block_map = item->block_maps[a][b];
                    Map *light_map = item->light_maps[a][b];
                    if (block_map) {
                        map_free(block_map);
                        free(block_map);
                    }
                    if (light_map) {
                        map_free(light_map);
                        free(light_map);
                    }
                }
            }
            worker->state = WORKER_IDLE;
        }
        mtx_unlock(&worker->mtx);
    }
}

void force_chunks(Player *player) {
    State *s = &player->state;
    int p = chunked(s->x);
    int q = chunked(s->z);
    int r = 1;
    for (int dp = -r; dp <= r; dp++) {
        for (int dq = -r; dq <= r; dq++) {
            int a = p + dp;
            int b = q + dq;
            Chunk *chunk = find_chunk(a, b);
            if (chunk) {
                if (chunk->dirty) {
                    gen_chunk_buffer(chunk);
                }
            }
            else if (g->chunk_count < MAX_CHUNKS) {
                chunk = g->chunks + g->chunk_count++;
                create_chunk(chunk, a, b);
                gen_chunk_buffer(chunk);
            }
        }
    }
}

void ensure_chunks_worker(Player *player, Worker *worker) {
    State *s = &player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    float planes[6][4];
    frustum_planes(planes, g->render_radius, matrix);
    int p = chunked(s->x);
    int q = chunked(s->z);
    int r = g->create_radius;
    int start = 0x0fffffff;
    int best_score = start;
    int best_a = 0;
    int best_b = 0;
    for (int dp = -r; dp <= r; dp++) {
        for (int dq = -r; dq <= r; dq++) {
            int a = p + dp;
            int b = q + dq;
            int index = (ABS(a) ^ ABS(b)) % WORKERS;
            if (index != worker->index) {
                continue;
            }
            Chunk *chunk = find_chunk(a, b);
            if (chunk && !chunk->dirty) {
                continue;
            }
            int distance = MAX(ABS(dp), ABS(dq));
            int invisible = !chunk_visible(planes, a, b, 0, 256);
            int priority = 0;
            if (chunk) {
                priority = chunk->buffer && chunk->dirty;
            }
            int score = (invisible << 24) | (priority << 16) | distance;
            if (score < best_score) {
                best_score = score;
                best_a = a;
                best_b = b;
            }
        }
    }
    if (best_score == start) {
        return;
    }
    int a = best_a;
    int b = best_b;
    int load = 0;
    Chunk *chunk = find_chunk(a, b);
    if (!chunk) {
        load = 1;
        if (g->chunk_count < MAX_CHUNKS) {
            chunk = g->chunks + g->chunk_count++;
            init_chunk(chunk, a, b);
        }
        else {
            return;
        }
    }
    WorkerItem *item = &worker->item;
    item->p = chunk->p;
    item->q = chunk->q;
    item->load = load;
    for (int dp = -1; dp <= 1; dp++) {
        for (int dq = -1; dq <= 1; dq++) {
            Chunk *other = chunk;
            if (dp || dq) {
                other = find_chunk(chunk->p + dp, chunk->q + dq);
            }
            if (other) {
                Map *block_map = malloc(sizeof(Map));
                map_copy(block_map, &other->map);
                Map *light_map = malloc(sizeof(Map));
                map_copy(light_map, &other->lights);
                item->block_maps[dp + 1][dq + 1] = block_map;
                item->light_maps[dp + 1][dq + 1] = light_map;
            }
            else {
                item->block_maps[dp + 1][dq + 1] = 0;
                item->light_maps[dp + 1][dq + 1] = 0;
            }
        }
    }
    chunk->dirty = 0;
    worker->state = WORKER_BUSY;
    cnd_signal(&worker->cnd);
}

void ensure_chunks(Player *player) {
    check_workers();
    force_chunks(player);
    for (int i = 0; i < WORKERS; i++) {
        Worker *worker = g->workers + i;
        mtx_lock(&worker->mtx);
        if (worker->state == WORKER_IDLE) {
            ensure_chunks_worker(player, worker);
        }
        mtx_unlock(&worker->mtx);
    }
}

int worker_run(void *arg) {
    Worker *worker = (Worker *)arg;
    while (!worker->exit_requested) {
        mtx_lock(&worker->mtx);
        while (worker->state != WORKER_BUSY) {
            cnd_wait(&worker->cnd, &worker->mtx);
            if (worker->exit_requested) {
                thrd_exit(1);
            }
        }
        mtx_unlock(&worker->mtx);
        WorkerItem *item = &worker->item;
        if (item->load) {
            load_chunk(item);
        }
        compute_chunk(item);
        mtx_lock(&worker->mtx);
        worker->state = WORKER_DONE;
        mtx_unlock(&worker->mtx);
    }
    return 0;
}

void unset_sign(int x, int y, int z) {
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        SignList *signs = &chunk->signs;
        if (sign_list_remove_all(signs, x, y, z)) {
            chunk->dirty = 1;
            db_delete_signs(x, y, z);
        }
    }
    else {
        db_delete_signs(x, y, z);
    }
}

void unset_sign_face(int x, int y, int z, int face) {
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        SignList *signs = &chunk->signs;
        if (sign_list_remove(signs, x, y, z, face)) {
            chunk->dirty = 1;
            db_delete_sign(x, y, z, face);
        }
    }
    else {
        db_delete_sign(x, y, z, face);
    }
}

void _set_sign(
    int p, int q, int x, int y, int z, int face, const char *text, int dirty)
{
    if (strlen(text) == 0) {
        unset_sign_face(x, y, z, face);
        return;
    }
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        SignList *signs = &chunk->signs;
        sign_list_add(signs, x, y, z, face, text);
        if (dirty) {
            chunk->dirty = 1;
        }
    }
    db_insert_sign(p, q, x, y, z, face, text);
}

void set_sign(int x, int y, int z, int face, const char *text) {
    int p = chunked(x);
    int q = chunked(z);
    _set_sign(p, q, x, y, z, face, text, 1);
    client_sign(x, y, z, face, text);
}

void toggle_light(int x, int y, int z) {
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->lights;
        int w = map_get(map, x, y, z) ? 0 : 15;
        map_set(map, x, y, z, w);
        db_insert_light(p, q, x, y, z, w);
        client_light(x, y, z, w);
        dirty_chunk(chunk);
    }
}

void set_light(int p, int q, int x, int y, int z, int w) {
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->lights;
        if (map_set(map, x, y, z, w)) {
            dirty_chunk(chunk);
            db_insert_light(p, q, x, y, z, w);
        }
    }
    else {
        db_insert_light(p, q, x, y, z, w);
    }
}

void _set_block(int p, int q, int x, int y, int z, int w, int dirty) {
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->map;
        if (map_set(map, x, y, z, w)) {
            if (dirty) {
                dirty_chunk(chunk);
            }
            db_insert_block(p, q, x, y, z, w);
        }
    }
    else {
        db_insert_block(p, q, x, y, z, w);
    }
    if (w == 0 && chunked(x) == p && chunked(z) == q) {
        unset_sign(x, y, z);
        set_light(p, q, x, y, z, 0);
    }
}

void set_block(int x, int y, int z, int w) {
    int p = chunked(x);
    int q = chunked(z);
    _set_block(p, q, x, y, z, w, 1);
    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            if (dx == 0 && dz == 0) {
                continue;
            }
            if (dx && chunked(x + dx) == p) {
                continue;
            }
            if (dz && chunked(z + dz) == q) {
                continue;
            }
            _set_block(p + dx, q + dz, x, y, z, -w, 1);
        }
    }
    client_block(x, y, z, w);
}

void record_block(int x, int y, int z, int w, LocalPlayer *local) {
    memcpy(&local->block1, &local->block0, sizeof(Block));
    local->block0.x = x;
    local->block0.y = y;
    local->block0.z = z;
    local->block0.w = w;
}

int get_block(int x, int y, int z) {
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->map;
        return map_get(map, x, y, z);
    }
    return 0;
}

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

int render_chunks(Attrib *attrib, Player *player) {
    int result = 0;
    State *s = &player->state;
    ensure_chunks(player);
    int p = chunked(s->x);
    int q = chunked(s->z);
    float light = get_daylight();
    float matrix[16];
    set_matrix_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    float planes[6][4];
    frustum_planes(planes, g->render_radius, matrix);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform3f(attrib->camera, s->x, s->y, s->z);
    glUniform1i(attrib->sampler, 0);
    glUniform1i(attrib->extra1, 2);
    glUniform1f(attrib->extra2, light);
    glUniform1f(attrib->extra3, g->render_radius * CHUNK_SIZE);
    glUniform1i(attrib->extra4, g->ortho);
    glUniform1f(attrib->timer, time_of_day());
    for (int i = 0; i < g->chunk_count; i++) {
        Chunk *chunk = g->chunks + i;
        if (chunk_distance(chunk, p, q) > g->render_radius) {
            continue;
        }
        if (!chunk_visible(
            planes, chunk->p, chunk->q, chunk->miny, chunk->maxy))
        {
            continue;
        }
        draw_chunk(attrib, chunk);
        result += chunk->faces;
    }
    return result;
}

void render_signs(Attrib *attrib, Player *player) {
    State *s = &player->state;
    int p = chunked(s->x);
    int q = chunked(s->z);
    float matrix[16];
    set_matrix_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    float planes[6][4];
    frustum_planes(planes, g->render_radius, matrix);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 3);
    glUniform1i(attrib->extra1, 1);
    for (int i = 0; i < g->chunk_count; i++) {
        Chunk *chunk = g->chunks + i;
        if (chunk_distance(chunk, p, q) > g->sign_radius) {
            continue;
        }
        if (!chunk_visible(
            planes, chunk->p, chunk->q, chunk->miny, chunk->maxy))
        {
            continue;
        }
        draw_signs(attrib, chunk);
    }
}

void render_sign(Attrib *attrib, LocalPlayer *local) {
    if (!local->typing || g->typing_buffer[0] != CRAFT_KEY_SIGN) {
        return;
    }
    int x, y, z, face;
    if (!hit_test_face(local->player, &x, &y, &z, &face)) {
        return;
    }
    State *s = &local->player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 3);
    glUniform1i(attrib->extra1, 1);
    char text[MAX_SIGN_LENGTH];
    strncpy(text, g->typing_buffer + 1, MAX_SIGN_LENGTH);
    text[MAX_SIGN_LENGTH - 1] = '\0';
    GLfloat *data = malloc_faces(5, strlen(text));
    int length = _gen_sign_buffer(data, x, y, z, face, text);
    GLuint buffer = gen_faces(5, length, data);
    draw_sign(attrib, buffer, length);
    del_buffer(buffer);
}

void render_players(Attrib *attrib, Player *player) {
    State *s = &player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform3f(attrib->camera, s->x, s->y, s->z);
    glUniform1i(attrib->sampler, 0);
    glUniform1f(attrib->timer, time_of_day());
    for (int i = 0; i < g->client_count; i++) {
        Client *client = g->clients + i;
        for (int j = 0; j < MAX_LOCAL_PLAYERS; j++) {
            Player *other = client->players + j;
            if (other != player && other->is_active) {
                draw_player(attrib, other);
            }
        }
    }
}

void render_sky(Attrib *attrib, Player *player, GLuint buffer) {
    State *s = &player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, g->width, g->height,
        0, 0, 0, s->rx, s->ry, g->fov, 0, g->render_radius);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 2);
    glUniform1f(attrib->timer, time_of_day());
    draw_triangles_3d_sky(attrib, buffer, 512 * 3);
}

void render_wireframe(Attrib *attrib, Player *player) {
    State *s = &player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (is_obstacle(hw)) {
        glUseProgram(attrib->program);
        glLineWidth(1);
        glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
        glUniform4fv(attrib->extra1, 1, BLACK);
        GLuint wireframe_buffer = gen_wireframe_buffer(hx, hy, hz, 0.53);
        draw_lines(attrib, wireframe_buffer, 3, 24);
        del_buffer(wireframe_buffer);
    }
}

void render_crosshairs(Attrib *attrib) {
    float matrix[16];
    set_matrix_2d(matrix, g->width, g->height);
    glUseProgram(attrib->program);
    glLineWidth(4 * g->scale);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform4fv(attrib->extra1, 1, BLACK);
    GLuint crosshair_buffer = gen_crosshair_buffer();
    draw_lines(attrib, crosshair_buffer, 2, 4);
    del_buffer(crosshair_buffer);
}

void render_item(Attrib *attrib, LocalPlayer *p) {
    float matrix[16];
    set_matrix_item(matrix, g->width, g->height, g->scale);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform3f(attrib->camera, 0, 0, 5);
    glUniform1i(attrib->sampler, 0);
    glUniform1f(attrib->timer, time_of_day());
    int w = items[p->item_index];
    if (is_plant(w)) {
        GLuint buffer = gen_plant_buffer(0, 0, 0, 0.5, w);
        draw_plant(attrib, buffer);
        del_buffer(buffer);
    }
    else {
        GLuint buffer = gen_cube_buffer(0, 0, 0, 0.5, w);
        draw_cube(attrib, buffer);
        del_buffer(buffer);
    }
}

void render_text(
    Attrib *attrib, int justify, float x, float y, float n, char *text)
{
    float matrix[16];
    set_matrix_2d(matrix, g->width, g->height);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 1);
    glUniform1i(attrib->extra1, 0);
    int length = strlen(text);
    x -= n * justify * (length - 1) / 2;
    GLuint buffer = gen_text_buffer(x, y, n, text);
    draw_text(attrib, buffer, length);
    del_buffer(buffer);
}

GLuint gen_text_cursor_buffer(float x, float y) {
    int p = 10 * g->scale;
    float data[] = {
        x, y - p, x, y + p,
    };
    return gen_buffer(sizeof(data), data);
}

void render_text_cursor(Attrib *attrib, float x, float y)
{
    float matrix[16];
    set_matrix_2d(matrix, g->width, g->height);
    glUseProgram(attrib->program);
    glLineWidth(2 * g->scale);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform4fv(attrib->extra1, 1, GREEN);
    GLuint text_cursor_buffer = gen_text_cursor_buffer(x, y);
    draw_lines(attrib, text_cursor_buffer, 2, 2);
    del_buffer(text_cursor_buffer);
}

void add_message(const char *text) {
    printf("%s\n", text);
    snprintf(
        g->messages[g->message_index], MAX_TEXT_LENGTH, "%s", text);
    g->message_index = (g->message_index + 1) % MAX_MESSAGES;
}

void login() {
    printf("Logging in anonymously\n");
    client_login("", "");
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

void parse_command(const char *buffer, int forward) {
    char server_addr[MAX_ADDR_LENGTH];
    int server_port = DEFAULT_PORT;
    char filename[MAX_PATH_LENGTH];
    char name[MAX_NAME_LENGTH];
    int int_option, radius, count, p, q, xc, yc, zc;
    char window_title[MAX_TITLE_LENGTH];
    LocalPlayer *local = g->keyboard_player;
    Player *player = g->keyboard_player->player;
    if (strcmp(buffer, "/fullscreen") == 0) {
        pg_toggle_fullscreen();
    }
    else if (sscanf(buffer,
        "/online %128s %d", server_addr, &server_port) >= 1)
    {
        g->mode_changed = 1;
        g->mode = MODE_ONLINE;
        strncpy(config->server, server_addr, MAX_ADDR_LENGTH);
        config->port = server_port;
        get_server_db_cache_path(g->db_path);
    }
    else if (sscanf(buffer, "/offline %128s", filename) == 1) {
        g->mode_changed = 1;
        g->mode = MODE_OFFLINE;
        snprintf(g->db_path, MAX_PATH_LENGTH, "%s/%s.piworld", config->path,
            filename);
    }
    else if (strcmp(buffer, "/offline") == 0) {
        g->mode_changed = 1;
        g->mode = MODE_OFFLINE;
        get_default_db_path(g->db_path);
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
            set_view_radius(radius);
        }
        else {
            add_message("Viewing distance must be between 0 and 24.");
        }
    }
    else if (sscanf(buffer, "/players %d", &int_option) == 1) {
        if (int_option >= 1 && int_option <= MAX_LOCAL_PLAYERS) {
            g->auto_match_players_to_joysticks = 0;
            if (config->players != int_option) {
                config->players = int_option;
                limit_player_count_to_fit_gpu_mem();
                move_primary_focus_to_active_player();
                set_player_count(g->clients, config->players);
                set_view_radius(g->render_radius);
            }
        }
        else {
            add_message("Player count must be between 1 and 4.");
        }
    }
    else if (strcmp(buffer, "/exit") == 0) {
        if (config->players <= 1) {
            terminate = True;
        } else {
            client_remove_player(player->id);
            player->is_active = 0;
            g->auto_match_players_to_joysticks = 0;
            config->players -= 1;
            move_primary_focus_to_active_player();
            set_players_view_size(g->width, g->height);
            set_view_radius(g->render_radius);
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
        config->show_clouds = int_option;
        g->mode_changed = 1;  // regenerate world
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
        config->show_lights = int_option;
        g->mode_changed = 1;  // regenerate world
    }
    else if (sscanf(buffer, "/show-plants %d", &int_option) == 1) {
        config->show_plants = int_option;
        g->mode_changed = 1;  // regenerate world
    }
    else if (sscanf(buffer, "/show-player-names %d", &int_option) == 1) {
        config->show_player_names = int_option;
    }
    else if (sscanf(buffer, "/show-trees %d", &int_option) == 1) {
        config->show_trees = int_option;
        g->mode_changed = 1;  // regenerate world
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
    else if (forward) {
        client_talk(buffer);
    }
}

void clear_block_under_crosshair(LocalPlayer *local)
{
    State *s = &local->player->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (hy > 0 && hy < 256 && is_destructable(hw)) {
        set_block(hx, hy, hz, 0);
        record_block(hx, hy, hz, 0, local);
        if (is_plant(get_block(hx, hy + 1, hz))) {
            set_block(hx, hy + 1, hz, 0);
        }
    }
}

void set_block_under_crosshair(LocalPlayer *local)
{
    State *s = &local->player->state;
    int hx, hy, hz;
    int hw = hit_test(1, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (hy > 0 && hy < 256 && is_obstacle(hw)) {
        if (!player_intersects_block(2, s->x, s->y, s->z, hx, hy, hz)) {
            set_block(hx, hy, hz, items[local->item_index]);
            record_block(hx, hy, hz, items[local->item_index], local);
        }
    }
}

void set_item_in_hand_to_item_under_crosshair(LocalPlayer *local)
{
    State *s = &local->player->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    for (int i = 0; i < item_count; i++) {
        if (items[i] == hw) {
            local->item_index = i;
            break;
        }
    }
}

void cycle_item_in_hand_up(LocalPlayer *player) {
    player->item_index = (player->item_index + 1) % item_count;
}

void cycle_item_in_hand_down(LocalPlayer *player) {
    player->item_index--;
    if (player->item_index < 0) {
        player->item_index = item_count - 1;
    }
}

void on_light(LocalPlayer *local) {
    State *s = &local->player->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (hy > 0 && hy < 256 && is_destructable(hw)) {
        toggle_light(hx, hy, hz);
    }
}

void on_left_click(LocalPlayer *local) {
    State *s = &local->player->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (hy > 0 && hy < 256 && is_destructable(hw)) {
        set_block(hx, hy, hz, 0);
        record_block(hx, hy, hz, 0, local);
        if (is_plant(get_block(hx, hy + 1, hz))) {
            set_block(hx, hy + 1, hz, 0);
        }
    }
}

void on_right_click(LocalPlayer *local) {
    State *s = &local->player->state;
    int hx, hy, hz;
    int hw = hit_test(1, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (hy > 0 && hy < 256 && is_obstacle(hw)) {
        if (!player_intersects_block(2, s->x, s->y, s->z, hx, hy, hz)) {
            set_block(hx, hy, hz, items[local->item_index]);
            record_block(hx, hy, hz, items[local->item_index], local);
        }
    }
}

void on_middle_click(LocalPlayer *local) {
    State *s = &local->player->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    for (int i = 0; i < item_count; i++) {
        if (items[i] == hw) {
            local->item_index = i;
            break;
        }
    }
}

void handle_mouse_input(LocalPlayer *local) {
    static double px = 0;
    static double py = 0;
    State *s = &local->player->state;
    if ((px || py)) {
        double mx, my;
        int mix, miy;
        get_x11_accumulative_mouse_motion(&mix, &miy);
        mx = (float)mix;
        my = (float)miy;
        float m = 0.0025;
        s->rx -= (mx - px) * m;
            s->ry += (my - py) * m;
        if (s->rx < 0) {
            s->rx += RADIANS(360);
        }
        if (s->rx >= RADIANS(360)){
            s->rx -= RADIANS(360);
        }
        s->ry = MAX(s->ry, -RADIANS(90));
        s->ry = MIN(s->ry, RADIANS(90));
        px = mx;
        py = my;
    }
    else {
        int pix, piy;
        get_x11_accumulative_mouse_motion(&pix, &piy);
        px = (float)pix;
        py = (float)piy;
    }
}

void handle_movement(double dt, LocalPlayer *local) {
    State *s = &local->player->state;
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
            int block_under_player_head = get_block(roundf(s->x), s->y,
                                                    roundf(s->z));
            if (is_obstacle(block_under_player_head)) {
                local->dy = 8;
            }
        }
    }
    float speed = 5;  // walking speed
    if (local->flying) {
        speed = 20;
    } else if (local->crouch_is_pressed) {
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
        if (local->crouch_is_pressed) {
            player_min_height = player_couching_height;
        }
        if (collide(player_min_height, &s->x, &s->y, &s->z)) {
            local->dy = 0;
        }
    }
    if (s->y < 0) {
        s->y = highest_block(s->x, s->z) + 2;
    }
}

void parse_buffer(char *buffer) {
    #define INVALID_PLAYER_INDEX (p < 1 || p > MAX_LOCAL_PLAYERS)
    Client *local_client = g->clients;
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
            if (!client && g->client_count < MAX_CLIENTS) {
                // Add a new client
                client = g->clients + g->client_count;
                g->client_count++;
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
                    printf("P now active: %d\n", p);
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
            force_chunks(me);
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
        if (sscanf(line, "L,%d,%d,%d,%d,%d,%d",
            &bp, &bq, &bx, &by, &bz, &bw) == 6)
        {
            set_light(bp, bq, bx, by, bz, bw);
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
            pg_set_time(fmod(elapsed, day_length));
            g->day_length = day_length;
            g->time_changed = 1;
            goto next_line;
        }
        if (line[0] == 'T' && line[1] == ',') {
            char *text = line + 2;
            add_message(text);
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

void reset_model() {
    memset(g->chunks, 0, sizeof(Chunk) * MAX_CHUNKS);
    g->chunk_count = 0;
    memset(g->clients, 0, sizeof(Client) * MAX_CLIENTS);
    g->client_count = 0;
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = &g->local_players[i];
        local->flying = 0;
        local->item_index = 0;
        local->typing = 0;
        local->observe1 = 0;
        local->observe1_client_id = 0;
        local->observe2 = 0;
        local->observe2_client_id = 0;
    }
    memset(g->typing_buffer, 0, sizeof(char) * MAX_TEXT_LENGTH);
    memset(g->messages, 0, sizeof(char) * MAX_MESSAGES * MAX_TEXT_LENGTH);
    g->message_index = 0;
    g->day_length = DAY_LENGTH;
    pg_set_time(g->day_length / 3.0);
    g->time_changed = 1;
    set_player_count(g->clients, config->players);
}

void insert_into_typing_buffer(unsigned char c) {
    int n = strlen(g->typing_buffer);
    if (n < MAX_TEXT_LENGTH - 1) {
        if (g->text_cursor != n) {
            // Shift text after the text cursor to the right
            memmove(g->typing_buffer + g->text_cursor + 1,
                    g->typing_buffer + g->text_cursor,
                    n - g->text_cursor);
        }
        g->typing_buffer[g->text_cursor] = c;
        g->typing_buffer[n + 1] = '\0';
        g->text_cursor += 1;
    }
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
        history->end = (history->end + 1) % MAX_HISTORY_SIZE;
        history->size = MIN(history->size + 1, MAX_HISTORY_SIZE);
        snprintf(history->lines[history->end], MAX_TEXT_LENGTH, "%s", line);
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

TextLineHistory* current_history()
{
    if (g->typing_buffer[0] == CRAFT_KEY_SIGN) {
        return &g->typing_history[SIGN_HISTORY];
    } else if (g->typing_buffer[0] == CRAFT_KEY_COMMAND) {
        return &g->typing_history[COMMAND_HISTORY];
    } else {
        return &g->typing_history[CHAT_HISTORY];
    }
}

void move_primary_focus_to_active_player()
{
    int focus = 0;
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        if (g->local_players[i].player->is_active) {
            focus = i;
            break;
        }
    }
    g->keyboard_player = &g->local_players[focus];
    g->mouse_player = &g->local_players[focus];
}

void cycle_primary_focus_around_local_players()
{
    int first_active_player = 0;
    for (int i=0; i<MAX_LOCAL_PLAYERS-1; i++) {
        LocalPlayer *local = &g->local_players[i];
        if (local->player->is_active) {
            first_active_player = i;
            break;
        }
    }
    int next = first_active_player;

    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = &g->local_players[i];
        if (i > (g->keyboard_player->player->id - 1)
            && local->player->is_active) {
            next = i;
            break;
        }
    }
    g->keyboard_player = &g->local_players[next];
    g->mouse_player = &g->local_players[next];
}

void handle_key_press(unsigned char c, int mods, int keysym)
{
    LocalPlayer *p = g->keyboard_player;
    if (!p->typing) {
        if (c == CRAFT_KEY_FORWARD) {
            p->forward_is_pressed = 1;
            p->movement_speed_forward_back = 1;
        } else if (c == CRAFT_KEY_BACKWARD) {
            p->back_is_pressed = 1;
            p->movement_speed_forward_back = 1;
        } else if (c == CRAFT_KEY_LEFT) {
            p->left_is_pressed = 1;
            p->movement_speed_left_right = 1;
        } else if (c == CRAFT_KEY_RIGHT) {
            p->right_is_pressed = 1;
            p->movement_speed_left_right = 1;
        } else if (keysym == XK_Escape) {
            terminate = True;
        } else if (keysym == XK_F1) {
            set_mouse_absolute();
        } else if (keysym == XK_F2) {
            set_mouse_relative();
        } else if (keysym == XK_F8) {
            cycle_primary_focus_around_local_players();
        } else if (keysym == XK_F11) {
            pg_toggle_fullscreen();
        } else if (keysym == XK_Up) {
            p->view_up_is_pressed = 1;
            p->view_speed_up_down = 1;
        } else if (keysym == XK_Down) {
            p->view_down_is_pressed = 1;
            p->view_speed_up_down = 1;
        } else if (keysym == XK_Left) {
            p->view_left_is_pressed = 1;
            p->view_speed_left_right = 1;
        } else if (keysym == XK_Right) {
            p->view_right_is_pressed = 1;
            p->view_speed_left_right = 1;
        } else if (c == ' ') {
            p->jump_is_pressed = 1;
        } else if (c == 'c') {
            p->crouch_is_pressed = 1;
        } else if (c == 'z') {
            p->zoom_is_pressed = 1;
        } else if (c == 'f') {
            p->ortho_is_pressed = 1;
        } else if (keysym == XK_Tab) {  // CRAFT_KEY_FLY
            p->flying = !p->flying;
        } else if (c >= '1' && c <= '9') {
            p->item_index = c - '1';
        } else if (c == '0' && c <= '9') {
            p->item_index = 9;
        } else if (c == CRAFT_KEY_ITEM_NEXT) {
            p->item_index = (p->item_index + 1) % item_count;
        } else if (c == CRAFT_KEY_ITEM_PREV) {
            p->item_index--;
            if (p->item_index < 0) {
                p->item_index = item_count - 1;
            }
        } else if (c == CRAFT_KEY_OBSERVE) {
            int start = ((p->observe1 == 0) ?  p->player->id : p->observe1) - 1;
            if (p->observe1_client_id == 0) {
                p->observe1_client_id = g->clients->id;
            }
            get_next_player(&start, &p->observe1_client_id);
            p->observe1 = start + 1;
            if (p->observe1 == p->player->id &&
                p->observe1_client_id == g->clients->id) {
                // cancel observing of another player
                p->observe1 = 0;
                p->observe1_client_id = 0;
            }
        } else if (c == CRAFT_KEY_OBSERVE_INSET) {
            int start = ((p->observe2 == 0) ?  p->player->id : p->observe2) - 1;
            if (p->observe2_client_id == 0) {
                p->observe2_client_id = g->clients->id;
            }
            get_next_player(&start, &p->observe2_client_id);
            p->observe2 = start + 1;
            if (p->observe2 == p->player->id &&
                p->observe2_client_id == g->clients->id) {
                // cancel observing of another player
                p->observe2 = 0;
                p->observe2_client_id = 0;
            }
        } else if (c == CRAFT_KEY_CHAT) {
            p->typing = 1;
            g->typing_buffer[0] = '\0';
            g->typing_start = g->typing_history[CHAT_HISTORY].line_start;
            g->text_cursor = g->typing_start;
        } else if (c == CRAFT_KEY_COMMAND) {
            p->typing = 1;
            g->typing_buffer[0] = '/';
            g->typing_buffer[1] = '\0';
            g->typing_start = g->typing_history[COMMAND_HISTORY].line_start;
            g->text_cursor = g->typing_start;
        } else if (c == CRAFT_KEY_SIGN) {
            p->typing = 1;
            g->typing_buffer[0] = CRAFT_KEY_SIGN;
            g->typing_buffer[1] = '\0';
            g->typing_start = g->typing_history[SIGN_HISTORY].line_start;
            g->text_cursor = g->typing_start;
        } else if (c == 13) {  // return
            if (mods & ControlMask) {
                on_right_click(g->mouse_player);
            } else {
                on_left_click(g->mouse_player);
            }
        }
    } else {
        if (keysym == XK_Escape) {
            p->typing = 0;
            g->history_position = NOT_IN_HISTORY;
        } else if (c == 8) {  // backspace
            int n = strlen(g->typing_buffer);
            if (n > 0 && g->text_cursor > g->typing_start) {
                if (g->text_cursor < n) {
                    memmove(g->typing_buffer + g->text_cursor - 1,
                            g->typing_buffer + g->text_cursor,
                            n - g->text_cursor);
                }
                g->typing_buffer[n - 1] = '\0';
                g->text_cursor -= 1;
            }
        } else if (c == 13) {  // return
            if (mods & ShiftMask) {
                insert_into_typing_buffer('\r');
            } else {
                p->typing = 0;
                if (g->typing_buffer[0] == CRAFT_KEY_SIGN) {
                    Player *player = g->clients->players;
                    int x, y, z, face;
                    if (hit_test_face(player, &x, &y, &z, &face)) {
                        set_sign(x, y, z, face, g->typing_buffer + 1);
                    }
                } else if (g->typing_buffer[0] == CRAFT_KEY_COMMAND) {
                    parse_command(g->typing_buffer, 1);
                } else {
                    client_talk(g->typing_buffer);
                }
                history_add(current_history(), g->typing_buffer);
                g->history_position = NOT_IN_HISTORY;
            }
        } else if (keysym == XK_Delete) {
            int n = strlen(g->typing_buffer);
            if (n > 0 && g->text_cursor < n) {
                memmove(g->typing_buffer + g->text_cursor,
                        g->typing_buffer + g->text_cursor + 1,
                        n - g->text_cursor);
                g->typing_buffer[n - 1] = '\0';
            }
        } else if (keysym == XK_Left) {
            if (g->text_cursor > g->typing_start) {
                g->text_cursor -= 1;
            }
        } else if (keysym == XK_Right) {
            if (g->text_cursor < strlen(g->typing_buffer)) {
                g->text_cursor += 1;
            }
        } else if (keysym == XK_Home) {
            g->text_cursor = g->typing_start;
        } else if (keysym == XK_End) {
            g->text_cursor = strlen(g->typing_buffer);
        } else if (keysym == XK_Up) {
            history_previous(current_history(), g->typing_buffer,
                             &g->history_position);
            g->text_cursor = strlen(g->typing_buffer);
        } else if (keysym == XK_Down) {
            history_next(current_history(), g->typing_buffer,
                         &g->history_position);
            g->text_cursor = strlen(g->typing_buffer);
        } else {
            if (c >= 32 && c < 128) {
                insert_into_typing_buffer(c);
            }
        }
    }
}

void handle_key_release(unsigned char c, int keysym)
{
    LocalPlayer *p = g->keyboard_player;
    if (c == CRAFT_KEY_FORWARD) {
        p->forward_is_pressed = 0;
    } else if (c == CRAFT_KEY_BACKWARD) {
        p->back_is_pressed = 0;
    } else if (c == CRAFT_KEY_LEFT) {
        p->left_is_pressed = 0;
    } else if (c == CRAFT_KEY_RIGHT) {
        p->right_is_pressed = 0;
    } else if (c == ' ') {
        p->jump_is_pressed = 0;
    } else if (c == 'c') {
        p->crouch_is_pressed = 0;
    } else if (c == 'z') {
        p->zoom_is_pressed = 0;
    } else if (c == 'f') {
        p->ortho_is_pressed = 0;
    } else if (keysym == XK_Up) {
        p->view_up_is_pressed = 0;
    } else if (keysym == XK_Down) {
        p->view_down_is_pressed = 0;
    } else if (keysym == XK_Left) {
        p->view_left_is_pressed = 0;
    } else if (keysym == XK_Right) {
        p->view_right_is_pressed = 0;
    }
}

void handle_mouse_release(int b, int mods)
{
    if (b == 1) {
        if (mods & ControlMask) {
            on_right_click(g->mouse_player);
        } else {
            on_left_click(g->mouse_player);
        }
    } else if (b == 2) {
        on_middle_click(g->mouse_player);
    } else if (b == 3) {
        if (mods & ControlMask) {
            on_light(g->mouse_player);
        } else {
            on_right_click(g->mouse_player);
        }
    } else if (b == 4) {
        g->keyboard_player->item_index =
            (g->keyboard_player->item_index + 1) % item_count;
    } else if (b == 5) {
        g->keyboard_player->item_index--;
        if (g->keyboard_player->item_index < 0) {
            g->keyboard_player->item_index = item_count - 1;
        }
    }
}

void handle_window_close()
{
    terminate = True;
}

void handle_focus_out() {
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *p = &g->local_players[i];
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

    pg_fullscreen(0);
}

void reset_history()
{
    g->history_position = NOT_IN_HISTORY;
    for (int i=0; i<NUM_HISTORIES; i++) {
        g->typing_history[i].end = -1;
        g->typing_history[i].size = 0;
    }
    g->typing_history[CHAT_HISTORY].line_start = 0;
    g->typing_history[COMMAND_HISTORY].line_start = 1;
    g->typing_history[SIGN_HISTORY].line_start = 1;
}

LocalPlayer *map_joystick_to_player(PG_Joystick *j, int j_num) {
    LocalPlayer *p = g->local_players;
    int joystick_count = pg_joystick_count();

    if (joystick_count < config->players) {
        // If 4 players and only 3 joysticks start the joystick mapping from
        // player 2, assuming player 1 still has the keyboard and mouse.
        j_num++;
    }

    if (j_num < config->players) {
        p = &g->local_players[j_num];
    }
    return p;
}

void handle_joystick_axis(PG_Joystick *j, int j_num, int axis, float value)
{
    LocalPlayer *p = map_joystick_to_player(j, j_num);

    if (j->axis_count < 4) {
        if (axis == 0) {
            p->left_is_pressed = (value < 0) ? 1 : 0;
            p->right_is_pressed = (value > 0) ? 1 : 0;
            p->movement_speed_left_right = fabs(value);
        } else if (axis == 1) {
            p->forward_is_pressed = (value < 0) ? 1 : 0;
            p->back_is_pressed = (value > 0) ? 1 : 0;
            p->movement_speed_forward_back = fabs(value);
        }
    } else {
        if (axis == 0) {
            p->movement_speed_left_right = fabs(value) * 2.0;
            p->left_is_pressed = (value < -DEADZONE);
            p->right_is_pressed = (value > DEADZONE);
        } else if (axis == 1) {
            p->movement_speed_forward_back = fabs(value) * 2.0;
            p->forward_is_pressed = (value < -DEADZONE);
            p->back_is_pressed = (value > DEADZONE);
        } else if (axis == 2) {
            p->view_speed_up_down = fabs(value) * 2.0;
            p->view_up_is_pressed = (value < -DEADZONE);
            p->view_down_is_pressed = (value > DEADZONE);
        } else if (axis == 3) {
            p->view_speed_left_right = fabs(value) * 2.0;
            p->view_left_is_pressed = (value < -DEADZONE);
            p->view_right_is_pressed = (value > DEADZONE);
        } else if (axis == 4) {
            p->movement_speed_left_right = fabs(value);
            p->left_is_pressed = (value < 0) ? 1 : 0;
            p->right_is_pressed = (value > 0) ? 1 : 0;
        } else if (axis == 5) {
            p->movement_speed_forward_back = fabs(value);
            p->forward_is_pressed = (value < 0) ? 1 : 0;
            p->back_is_pressed = (value > 0) ? 1 : 0;
        }
    }
}

void handle_joystick_button(PG_Joystick *j, int j_num, int button, int state)
{
    LocalPlayer *p = map_joystick_to_player(j, j_num);

    if (j->axis_count < 4) {
        if (button == 0) {
            p->view_up_is_pressed = state;
            p->view_speed_up_down = 1;
        } else if (button == 2) {
            p->view_down_is_pressed = state;
            p->view_speed_up_down = 1;
        } else if (button == 3) {
            p->view_left_is_pressed = state;
            p->view_speed_left_right = 1;
        } else if (button == 1) {
            p->view_right_is_pressed = state;
            p->view_speed_left_right = 1;
        } else if (button == 5) {
            switch (p->shoulder_button_mode) {
                case CROUCH_JUMP:
                    p->jump_is_pressed = state;
                    break;
                case REMOVE_ADD:
                    if (state) {
                        set_block_under_crosshair(p);
                    }
                    break;
                case CYCLE_DOWN_UP:
                    if (state) {
                        cycle_item_in_hand_up(p);
                    }
                    break;
                case FLY_PICK:
                    if (state) {
                        set_item_in_hand_to_item_under_crosshair(p);
                    }
                    break;
                case ZOOM_ORTHO:
                    p->ortho_is_pressed = state;
                    break;
            }
        } else if (button == 4) {
           switch (p->shoulder_button_mode) {
                case CROUCH_JUMP:
                    p->crouch_is_pressed = state;
                    break;
                case REMOVE_ADD:
                    if (state) {
                        clear_block_under_crosshair(p);
                    }
                    break;
                case CYCLE_DOWN_UP:
                    if (state) {
                        cycle_item_in_hand_down(p);
                    }
                    break;
                case FLY_PICK:
                    if (state) {
                        p->flying = !p->flying;
                    }
                    break;
                case ZOOM_ORTHO:
                    p->zoom_is_pressed = state;
                    break;
            }
        } else if (button == 8) {
            if (state) {
                p->shoulder_button_mode += 1;
                p->shoulder_button_mode %= SHOULDER_BUTTON_MODE_COUNT;
                add_message(shoulder_button_modes[p->shoulder_button_mode]);
            }
        }
    } else {
        if (button == 0) {
            if (state) {
                cycle_item_in_hand_up(p);
            }
        } else if (button == 2) {
            if (state) {
                cycle_item_in_hand_down(p);
            }
        } else if (button == 3) {
            if (state) {
                set_item_in_hand_to_item_under_crosshair(p);
            }
        } else if (button == 1) {
            if (state) {
                p->flying = !p->flying;
            }
        } else if (button == 5) {
            if (state) {
                clear_block_under_crosshair(p);
            }
        } else if (button == 4) {
            p->crouch_is_pressed = state;
        } else if (button == 6) {
            p->jump_is_pressed = state;
        } else if (button == 7) {
            if (state) {
                set_block_under_crosshair(p);
            }
        } else if (button == 8) {
            p->zoom_is_pressed = state;
        } else if (button == 9) {
            p->ortho_is_pressed = state;
        } else if (button == 10) {
            p->ortho_is_pressed = state;
        } else if (button == 11) {
            p->zoom_is_pressed = state;
        }
    }
}

void render_player_world(
        LocalPlayer *local, GLuint sky_buffer, Attrib sky_attrib,
        Attrib block_attrib, Attrib text_attrib, Attrib line_attrib,
        FPS fps)
{
    Player *player = local->player;
    State *s = &player->state;

    glViewport(local->view_x, local->view_y, local->view_width,
               local->view_height);
    g->width = local->view_width;
    g->height = local->view_height;
    g->ortho = local->ortho_is_pressed ? 64 : 0;
    g->fov = local->zoom_is_pressed ? 15 : 65;

    if (local->observe1 > 0 && find_client(local->observe1_client_id)) {
        player = find_client(local->observe1_client_id)->players +
                 (local->observe1 - 1);
    }

    // RENDER 3-D SCENE //
    render_sky(&sky_attrib, player, sky_buffer);
    glClear(GL_DEPTH_BUFFER_BIT);
    int face_count = render_chunks(&block_attrib, player);
    render_signs(&text_attrib, player);
    render_sign(&text_attrib, local);
    render_players(&block_attrib, player);
    if (config->show_wireframe) {
        render_wireframe(&line_attrib, player);
    }

    // RENDER HUD //
    glClear(GL_DEPTH_BUFFER_BIT);
    if (config->show_crosshairs) {
        render_crosshairs(&line_attrib);
    }
    if (config->show_item) {
        render_item(&block_attrib, local);
    }

    // RENDER TEXT //
    char text_buffer[1024];
    float ts = 12 * g->scale;
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
                g->client_count, g->chunk_count,
                face_count * 2, hour, am_pm);
            render_text(&text_attrib, ALIGN_LEFT, tx, ty, ts,
                        text_buffer);
            ty -= ts * 2;

            // FPS counter in lower right corner
            float bottom_bar_y = 0 + ts * 2;
            float right_side = g->width - ts;
            snprintf(text_buffer, 1024, "%dfps", fps.fps);
            render_text(&text_attrib, ALIGN_RIGHT, right_side,
                        bottom_bar_y, ts, text_buffer);
        } else {
            snprintf(text_buffer, 1024, "(%.2f, %.2f, %.2f)",
                     s->x, s->y, s->z);
            render_text(&text_attrib, ALIGN_LEFT, tx, ty, ts,
                        text_buffer);

            // Game time in upper right corner
            float right_side = g->width - tx;
            snprintf(text_buffer, 1024, "%d%cm", hour, am_pm);
            render_text(&text_attrib, ALIGN_RIGHT, right_side, ty, ts,
                        text_buffer);

            ty -= ts * 2;
        }
    }
    if (config->show_chat_text) {
        for (int i = 0; i < MAX_MESSAGES; i++) {
            int index = (g->message_index + i) % MAX_MESSAGES;
            if (strlen(g->messages[index])) {
                render_text(&text_attrib, ALIGN_LEFT, tx, ty, ts,
                    g->messages[index]);
                ty -= ts * 2;
            }
        }
    }
    if (local->typing) {
        snprintf(text_buffer, 1024, "> %s", g->typing_buffer);
        render_text(&text_attrib, ALIGN_LEFT, tx, ty, ts, text_buffer);
        glClear(GL_DEPTH_BUFFER_BIT);
        render_text_cursor(&line_attrib, tx + ts * (g->text_cursor+1) + ts/2,
                           ty);
        ty -= ts * 2;
    }
    if (config->show_player_names) {
        Player *other = player_crosshair(player);
        if (other) {
            render_text(&text_attrib, ALIGN_CENTER,
                g->width / 2, g->height / 2 - ts - 24, ts,
                other->name);
        }
    }
    if (config->players > 1) {
        // Render player name if more than 1 local player
        if (player == g->keyboard_player->player) {
            snprintf(text_buffer, 1024, "* %s *", player->name);
        } else {
            snprintf(text_buffer, 1024, "%s", player->name);
        }
        render_text(&text_attrib, ALIGN_CENTER, g->width/2, ts, ts,
                    text_buffer);
    }

    // RENDER PICTURE IN PICTURE //
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

        render_sky(&sky_attrib, player, sky_buffer);
        glClear(GL_DEPTH_BUFFER_BIT);
        render_chunks(&block_attrib, player);
        render_signs(&text_attrib, player);
        render_players(&block_attrib, player);
        glClear(GL_DEPTH_BUFFER_BIT);
        if (config->show_player_names) {
            render_text(&text_attrib, ALIGN_CENTER,
                pw / 2, ts, ts, player->name);
        }
    }
}

void set_players_view_size(int w, int h)
{
    int view_margin = 6;
    int active_count = 0;
    for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
        LocalPlayer *local = &g->local_players[i];
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

void set_players_to_match_joysticks()
{
    int joystick_count = pg_joystick_count();
    if (joystick_count != config->players) {
        config->players = MAX(1, pg_joystick_count());
        config->players = MIN(MAX_LOCAL_PLAYERS, config->players);
        limit_player_count_to_fit_gpu_mem();
        set_player_count(g->clients, config->players);
    }
}

/*
 * Set view radius that will fit into the current size of GPU RAM.
 */
void set_view_radius(int requested_size)
{
    int radius = requested_size;
    int extend_delete_radius = 3;
    int gpu_mb = pg_get_gpu_mem_size();
    if (gpu_mb < 48 || (gpu_mb < 128 && config->players >= 2)) {
        // A draw distance of 1 is not enough for the game to be usable, but
        // this does at least show something on screen (for low resolutions
        // only - higher ones will crash the game with low GPU RAM).
        radius = 1;
        extend_delete_radius = 1;
    } else if (gpu_mb < 64) {
        radius = 2;
        extend_delete_radius = 1;
    } else if (gpu_mb < 128 || (gpu_mb < 256 && config->players >= 3)) {
        // A GPU RAM size of 64M will result in rendering issues for draw
        // distances greater than 3 (with a chunk size of 16).
        radius = 3;
        extend_delete_radius = 1;
    } else if (gpu_mb < 256 || requested_size == AUTO_PICK_VIEW_RADIUS) {
        // For the Raspberry Pi reduce amount to draw to both fit into 128MiB
        // of GPU RAM and keep the render speed at a reasonable smoothness.
        radius = 5;
    }

    g->create_radius = radius;
    g->render_radius = radius;
    g->delete_radius = radius + extend_delete_radius;
    g->sign_radius = radius;

    if (config->verbose) {
        printf("\nradii: create: %d render: %d delete: %d sign: %d\n",
               g->create_radius, g->render_radius, g->delete_radius,
               g->sign_radius);
    }
}

int main(int argc, char **argv) {
    // INITIALIZATION //
    srand(time(NULL));
    rand();
    reset_config();
    reset_history();
    parse_startup_config(argc, argv);

    // WINDOW INITIALIZATION //
    g->width = config->window_width;
    g->height = config->window_height;
    pg_start(config->window_title, config->window_x, config->window_y,
             g->width, g->height);
    pg_swap_interval(config->vsync);
    set_key_press_handler(*handle_key_press);
    set_key_release_handler(*handle_key_release);
    set_mouse_release_handler(*handle_mouse_release);
    set_window_close_handler(*handle_window_close);
    set_focus_out_handler(*handle_focus_out);
    if (config->verbose) {
        pg_print_info();
    }
    pg_init_joysticks();
    if (config->players == -1) {
        g->auto_match_players_to_joysticks = 1;
        set_players_to_match_joysticks();
    } else {
        g->auto_match_players_to_joysticks = 0;
        limit_player_count_to_fit_gpu_mem();
        set_player_count(g->clients, config->players);
    }
    if (config->fullscreen) {
        pg_fullscreen(1);
    }
    set_view_radius(config->view);

    pg_set_joystick_button_handler(*handle_joystick_button);
    pg_set_joystick_axis_handler(*handle_joystick_axis);

    g->keyboard_player = g->local_players;
    g->mouse_player = g->local_players;

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0, 0, 0, 1);

    // LOAD TEXTURES //
    GLuint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    load_png_texture("textures/texture.png");

    GLuint font;
    glGenTextures(1, &font);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, font);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    load_png_texture("textures/font.png");

    GLuint sky;
    glGenTextures(1, &sky);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, sky);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    load_png_texture("textures/sky.png");

    GLuint sign;
    glGenTextures(1, &sign);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, sign);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    load_png_texture("textures/sign.png");

    // LOAD SHADERS //
    Attrib block_attrib = {0};
    Attrib line_attrib = {0};
    Attrib text_attrib = {0};
    Attrib sky_attrib = {0};
    GLuint program;

    program = load_program(
        "shaders/block_vertex.glsl", "shaders/block_fragment.glsl");
    block_attrib.program = program;
    block_attrib.position = glGetAttribLocation(program, "position");
    block_attrib.normal = glGetAttribLocation(program, "normal");
    block_attrib.uv = glGetAttribLocation(program, "uv");
    block_attrib.matrix = glGetUniformLocation(program, "matrix");
    block_attrib.sampler = glGetUniformLocation(program, "sampler");
    block_attrib.extra1 = glGetUniformLocation(program, "sky_sampler");
    block_attrib.extra2 = glGetUniformLocation(program, "daylight");
    block_attrib.extra3 = glGetUniformLocation(program, "fog_distance");
    block_attrib.extra4 = glGetUniformLocation(program, "ortho");
    block_attrib.camera = glGetUniformLocation(program, "camera");
    block_attrib.timer = glGetUniformLocation(program, "timer");

    program = load_program(
        "shaders/line_vertex.glsl", "shaders/line_fragment.glsl");
    line_attrib.program = program;
    line_attrib.position = glGetAttribLocation(program, "position");
    line_attrib.matrix = glGetUniformLocation(program, "matrix");
    line_attrib.extra1 = glGetUniformLocation(program, "color");

    program = load_program(
        "shaders/text_vertex.glsl", "shaders/text_fragment.glsl");
    text_attrib.program = program;
    text_attrib.position = glGetAttribLocation(program, "position");
    text_attrib.uv = glGetAttribLocation(program, "uv");
    text_attrib.matrix = glGetUniformLocation(program, "matrix");
    text_attrib.sampler = glGetUniformLocation(program, "sampler");
    text_attrib.extra1 = glGetUniformLocation(program, "is_sign");

    program = load_program(
        "shaders/sky_vertex.glsl", "shaders/sky_fragment.glsl");
    sky_attrib.program = program;
    sky_attrib.position = glGetAttribLocation(program, "position");
    sky_attrib.uv = glGetAttribLocation(program, "uv");
    sky_attrib.matrix = glGetUniformLocation(program, "matrix");
    sky_attrib.sampler = glGetUniformLocation(program, "sampler");
    sky_attrib.timer = glGetUniformLocation(program, "timer");

    // ONLINE STATUS //
    if (strlen(config->server) > 0) {
        g->mode = MODE_ONLINE;
        get_server_db_cache_path(g->db_path);
    } else {
        g->mode = MODE_OFFLINE;
        snprintf(g->db_path, MAX_PATH_LENGTH, "%s", config->db_path);
    }

    // OUTER LOOP //
    int running = 1;
    while (running) {

        // INITIALIZE WORKER THREADS
        for (int i = 0; i < WORKERS; i++) {
            Worker *worker = g->workers + i;
            worker->index = i;
            worker->state = WORKER_IDLE;
            worker->exit_requested = False;
            mtx_init(&worker->mtx, mtx_plain);
            cnd_init(&worker->cnd);
            thrd_create(&worker->thrd, worker_run, worker);
        }

        // DATABASE INITIALIZATION //
        if (g->mode == MODE_OFFLINE || config->use_cache) {
            db_enable();
            if (db_init(g->db_path)) {
                return -1;
            }
            if (g->mode == MODE_ONLINE) {
                // TODO: support proper caching of signs (handle deletions)
                db_delete_all_signs();
            }
        }

        // CLIENT INITIALIZATION //
        if (g->mode == MODE_ONLINE) {
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
        GLuint sky_buffer = gen_sky_buffer();

        g->client_count = 1;
        g->clients->id = 0;

        for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
            g->local_players[i].player = &g->clients->players[i];
            LocalPlayer *local = &g->local_players[i];
            State *s = &local->player->state;

            local->player->id = i+1;
            local->player->name[0] = '\0';
            local->player->buffer = 0;
            local->player->texture_index = i;

            // LOAD STATE FROM DATABASE //
            int loaded = db_load_state(&s->x, &s->y, &s->z, &s->rx, &s->ry, i);
            force_chunks(local->player);
            if (!loaded) {
                s->y = highest_block(s->x, s->z) + 2;
            }

            loaded = db_load_player_name(local->player->name, MAX_NAME_LENGTH, i);
            if (!loaded) {
                snprintf(local->player->name, MAX_NAME_LENGTH, "player%d", i + 1);
            }
        }

        for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
            Player *player = g->local_players[i].player;
            if (player->is_active) {
                client_add_player(player->id);
            }
        }

        // VIEW SETUP //
        g->ortho = 0;
        g->fov = 65;

        // BEGIN MAIN LOOP //
        double previous = pg_get_time();
        while (!terminate) {
            // WINDOW SIZE AND SCALE //
            g->scale = get_scale_factor();
            pg_get_window_size(&g->width, &g->height);
            glViewport(0, 0, g->width, g->height);
            set_players_view_size(g->width, g->height);

            // FRAME RATE //
            if (g->time_changed) {
                g->time_changed = 0;
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

            // HANDLE MOUSE INPUT //
            handle_mouse_input(g->mouse_player);

            // HANDLE MOVEMENT //
            for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
                LocalPlayer *local = &g->local_players[i];
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
                    LocalPlayer *local = &g->local_players[i];
                    if (local->player->is_active) {
                        State *s = &local->player->state;
                        client_position(local->player->id,
                                        s->x, s->y, s->z, s->rx, s->ry);
                    }
                }
            }

            // PREPARE TO RENDER //
            delete_chunks();
            for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
                Player *player = g->local_players[i].player;
                if (player->is_active) {
                    State *s = &player->state;
                    del_buffer(player->buffer);
                    player->buffer = gen_player_buffer(s->x, s->y, s->z, s->rx,
                        s->ry, player->texture_index);
                }
            }
            for (int i = 1; i < g->client_count; i++) {
                Client *client = g->clients + i;
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
                LocalPlayer *local = &g->local_players[i];
                if (local->player->is_active) {
                    render_player_world(local, sky_buffer,
                                        sky_attrib, block_attrib, text_attrib,
                                        line_attrib, fps);
                }
            }

            // Match player count to joystick count
            static int players_joysticks_counter = 0;
            #define PLAYER_JOYSTICK_RATE_LIMIT 100
            if (g->auto_match_players_to_joysticks == 1) {
                if (players_joysticks_counter > PLAYER_JOYSTICK_RATE_LIMIT) {
                    int prev_player_count = config->players;
                    set_players_to_match_joysticks();
                    if (config->players != prev_player_count) {
                        set_view_radius(g->render_radius);
                    }
                    players_joysticks_counter = 0;
                }
                players_joysticks_counter++;
            }

#ifdef DEBUG
            check_gl_error();
#endif

            // SWAP AND POLL //
            pg_swap_buffers();
            pg_next_event();
            if (g->mode_changed) {
                g->mode_changed = 0;
                break;
            }
        }
        if (terminate) {
            running = 0;
        }

        // DEINITIALIZE WORKER THREADS
        // Stop thread processing
        for (int i = 0; i < WORKERS; i++) {
            Worker *worker = g->workers + i;
            worker->exit_requested = True;
            cnd_signal(&worker->cnd);
        }
        // Wait for worker threads to exit
        for (int i = 0; i < WORKERS; i++) {
            Worker *worker = g->workers + i;
            thrd_join(worker->thrd, NULL);
            cnd_destroy(&worker->cnd);
            mtx_destroy(&worker->mtx);
        }

        // SHUTDOWN //
        db_clear_state();
        db_clear_player_names();
        for (int i=0; i<MAX_LOCAL_PLAYERS; i++) {
            State *ls = &g->local_players[i].player->state;
            db_save_state(ls->x, ls->y, ls->z, ls->rx, ls->ry);
            db_save_player_name(g->local_players[i].player->name);
        }
        db_close();
        db_disable();
        client_stop();
        client_disable();
        del_buffer(sky_buffer);
        delete_all_chunks();
        delete_all_players();
    }

    pg_terminate_joysticks();
    pg_end();
    return 0;
}

