#pragma once

#include "tinycthread.h"

int chunked(float x);
int get_block(int x, int y, int z);
void set_block(int x, int y, int z, int w);
void add_message(const char *text);
void pw_get_player_pos(int pid, float *x, float *y, float *z);
void pw_set_player_pos(int pid, float x, float y, float z);
void pw_get_player_angle(int pid, float *x, float *y);
void pw_set_player_angle(int pid, float x, float y);
int pw_get_crosshair(int pid, int *hx, int *hy, int *hz, int *face);
void set_sign(int x, int y, int z, int face, const char *text);
int pw_get_time(void);
void pw_set_time(int time);

extern mtx_t force_chunks_mtx;

