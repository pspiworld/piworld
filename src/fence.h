#pragma once

#include "door.h"

void fence_init(void);
int fence_face_count(int shape);

void make_fence(
    float *data, float ao[6][4], float light[6][4],
    int left, int right, int top, int bottom, int front, int back,
    float x, float y, float z, float n, int w, int shape, int extra,
    int rotate);

void gate_toggle_open(DoorMapEntry *gate, int x, int y,
    int z, GLuint buffer, size_t float_size);

