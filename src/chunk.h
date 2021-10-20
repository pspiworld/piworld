#pragma once

#include <GLES2/gl2.h>
#include "door.h"
#include "map.h"
#include "player.h"
#include "pwlua.h"
#include "sign.h"
#include "tinycthread.h"

#define WORKERS 1

#define WORKER_IDLE 0
#define WORKER_BUSY 1
#define WORKER_DONE 2

typedef struct {
    Map map;
    Map extra;
    Map lights;
    Map shape;
    SignList signs;
    Map transform;
    DoorMap doors;
    int p;
    int q;
    int faces;
    int sign_faces;
    int dirty;
    int dirty_signs;
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
    Map *extra_maps[3][3];
    Map *light_maps[3][3];
    Map *shape_maps[3][3];
    Map *transform_maps[3][3];
    DoorMap *door_maps[3][3];
    SignList signs;
    int miny;
    int maxy;
    int faces;
    void *data;
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

int chunked(float x);
int chunk_distance(Chunk *chunk, int p, int q);
int chunk_visible(float planes[6][4], int p, int q, int miny, int maxy,
    int ortho);
void load_chunk(WorkerItem *item, lua_State *L);
void create_chunk(Chunk *chunk, int p, int q);
void request_chunk(int p, int q);
void compute_chunk(WorkerItem *item);
void generate_chunk(Chunk *chunk, WorkerItem *item, size_t float_size);
void ensure_chunks_worker(Player *player, Worker *worker, int width,
    int height, int fov, int ortho, int render_radius, int create_radius);
void gen_chunk_buffer(Chunk *chunk, size_t float_size);
void force_chunks(Player *player, size_t float_size);

