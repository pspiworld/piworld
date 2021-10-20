#pragma once
/*
 * DoorMap keeps data that will be used to generate a door shape (by calling
 * make_door). We keep this data around after the initial door blocks have been
 * made so that when opening/closing a door the, already created, GL data can
 * be edited with the new door state without having to regenerate the entire
 * data set for that chunk.
 */
#include <stddef.h>

#define DOOR_EMPTY_ENTRY(entry) ((entry)->value == 0)

#define DOOR_MAP_FOR_EACH(map, ex, ey, ez, ew) \
    for (unsigned int i = 0; i <= map->mask; i++) { \
        DoorMapEntry *entry = map->data + i; \
        if (DOOR_EMPTY_ENTRY(entry)) { \
            continue; \
        } \
        int ex = entry->e.x + map->dx; \
        int ey = entry->e.y + map->dy; \
        int ez = entry->e.z + map->dz; \
        int ew = entry->e.w;

#define END_DOOR_MAP_FOR_EACH }

typedef struct {
    union {
        unsigned int value;
        struct {
            unsigned char x;
            unsigned char y;
            unsigned char z;
            signed char w;
        } e;
    };
    int offset_into_gl_buffer;
    int face_count_in_gl_buffer;
    float ao[6][4];
    float light[6][4];
    int left;
    int right;
    int top;
    int bottom;
    int front;
    int back;
    float n;
    int shape;
    int extra;
    int transform;
} DoorMapEntry;

typedef struct {
    int dx;
    int dy;
    int dz;
    unsigned int mask;
    unsigned int size;
    DoorMapEntry *data;
} DoorMap;

#define POS_PIX (0.0625 * 2)
#define P00 -1
#define P01 (P00 + POS_PIX)
#define P02 (P00 + POS_PIX * 2)
#define P03 (P00 + POS_PIX * 3)
#define P04 (P00 + POS_PIX * 4)
#define P05 (P00 + POS_PIX * 5)
#define P06 (P00 + POS_PIX * 6)
#define P07 (P00 + POS_PIX * 7)
#define P08 (P00 + POS_PIX * 8)
#define P09 (P00 + POS_PIX * 9)
#define P10 (P00 + POS_PIX * 10)
#define P11 (P00 + POS_PIX * 11)
#define P12 (P00 + POS_PIX * 12)
#define P13 (P00 + POS_PIX * 13)
#define P14 (P00 + POS_PIX * 14)
#define P15 (P00 + POS_PIX * 15)
#define P16 +1

#define UV01 (1 / 2048.0)
#define UV14 (14 / 256.0)
#define UV16 (1 / 16.0 - 1 / 2048.0)

void door_map_alloc(DoorMap *map, int dx, int dy, int dz, int mask);
void door_map_free(DoorMap *map);
void door_map_copy(DoorMap *dst, DoorMap *src);
void door_map_grow(DoorMap *map);
int door_map_set(DoorMap *map, int x, int y, int z, int w,
    int offset_into_gl_buffer, int face_count_in_gl_buffer, float ao[6][4],
    float light[6][4], int left, int right, int top,
    int bottom, int front, int back, float n, int shape,
    int extra, int transform);
void door_map_clear(DoorMap *map, int x, int y, int z);
DoorMapEntry *door_map_get(DoorMap *map, int x, int y, int z);

void make_door(
    float *data, float ao[6][4], float light[6][4],
    int left, int right, int top, int bottom, int front, int back,
    float x, float y, float z, float n, int w, int shape, int extra,
    int transform);

void door_toggle_open(DoorMap *door_map, DoorMapEntry *door, int x, int y,
    int z, GLuint buffer);
