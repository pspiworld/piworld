#pragma once

#include <GLES2/gl2.h>
#include "sign.h"
#include "tinycthread.h"

#define ALIGN_LEFT 0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT 2

typedef struct {
    GLuint program;
    GLuint position;
    GLuint normal;
    GLuint uv;
    GLuint color;
    GLuint matrix;
    GLuint sampler;
    GLuint camera;
    GLuint timer;
    GLuint extra1;
    GLuint extra2;
    GLuint extra3;
    GLuint extra4;
    GLuint extra5;
    GLuint extra6;
    GLuint map;
} Attrib;

int chunked(float x);
int get_block(int x, int y, int z);
void set_block(int x, int y, int z, int w);
int get_extra(int x, int y, int z);
void set_extra(int x, int y, int z, int w);
void set_extra_non_dirty(int x, int y, int z, int w);
int get_shape(int x, int y, int z);
void set_shape(int x, int y, int z, int w);
int get_transform(int x, int y, int z);
void set_transform(int x, int y, int z, int w);
void queue_set_block(int x, int y, int z, int w);
void queue_set_extra(int x, int y, int z, int w);
void queue_set_light(int x, int y, int z, int w);
void queue_set_shape(int x, int y, int z, int w);
void queue_set_sign(int x, int y, int z, int face, const char *text);
void queue_set_transform(int x, int y, int z, int w);
void add_message(int player_id, const char *text);
void pw_get_player_pos(int pid, float *x, float *y, float *z);
void pw_set_player_pos(int pid, float x, float y, float z);
void pw_get_player_angle(int pid, float *x, float *y);
void pw_set_player_angle(int pid, float x, float y);
int pw_get_crosshair(int pid, int *hx, int *hy, int *hz, int *face);
const unsigned char *get_sign(int p, int q, int x, int y, int z, int face);
void set_sign(int x, int y, int z, int face, const char *text);
void worldgen_set_sign(int x, int y, int z, int face, const char *text,
                       SignList *sign_list);
int pw_get_time(void);
void pw_set_time(int time);
int get_light(int p, int q, int x, int y, int z);
void set_light(int p, int q, int x, int y, int z, int w);
void map_set_func(int x, int y, int z, int w, void *arg);
void render_text_rgba(
    Attrib *attrib, int justify, float x, float y, float n, char *text,
    const float *background, const float *text_color);
void render_text_cursor(Attrib *attrib, float x, float y);
void drain_edit_queue(size_t max_items, double max_time, double now);
