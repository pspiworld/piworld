#pragma once

#include "chunk.h"
#include "client.h"
#include "player.h"

#define MAX_CHUNKS 8192

extern Chunk chunks[MAX_CHUNKS];
extern int chunk_count;

void chunks_reset(void);
int hit_test(
    int previous, float x, float y, float z, float rx, float ry,
    int *bx, int *by, int *bz);
int hit_test_face(Player *player, int *x, int *y, int *z, int *face);
int get_next_local_player(Client *client, int start);
Chunk *find_chunk(int p, int q);
void dirty_chunk(Chunk *chunk);
Chunk *next_available_chunk(void);
void toggle_light(int x, int y, int z);
int collide(int height, float *x, float *y, float *z, float *ydiff);
int highest_block(float x, float z);
int get_block(int x, int y, int z);
void set_block(int x, int y, int z, int w);
int get_light(int p, int q, int x, int y, int z);
void set_light(int p, int q, int x, int y, int z, int w);
int get_extra(int x, int y, int z);
void set_extra(int x, int y, int z, int w);
void set_extra_non_dirty(int x, int y, int z, int w);
int get_shape(int x, int y, int z);
void set_shape(int x, int y, int z, int w);
int get_transform(int x, int y, int z);
void set_transform(int x, int y, int z, int w);
void _set_block(int p, int q, int x, int y, int z, int w, int dirty);
void _set_extra(int p, int q, int x, int y, int z, int w, int dirty);
void _set_shape(int p, int q, int x, int y, int z, int w, int dirty);
void _set_transform(int p, int q, int x, int y, int z, int w, int dirty);
int _set_light(int p, int q, int x, int y, int z, int w);
void _set_sign(
    int p, int q, int x, int y, int z, int face, const char *text, int dirty);
void delete_chunks(int delete_radius);
void delete_all_chunks(void);
void benchmark_chunks(int count);
