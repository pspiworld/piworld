#pragma once
/*
 * DoorMap keeps data that will be used to generate a door shape (by calling
 * make_door). We keep this data around after the initial door blocks have been
 * made so that when opening/closing a door the, already created, GL data can
 * be edited with the new door state without having to regenerate the entire
 * data set for that chunk.
 */

#define DOOR_EMPTY_ENTRY(entry) \
    ((entry)->x == 0 && (entry)->y == 0 && (entry)->y == 0 && (entry)->w == 0)

#define DOOR_MAP_FOR_EACH(map, ex, ey, ez, ew) \
    for (unsigned int i = 0; i <= map->mask; i++) { \
        DoorMapEntry *entry = map->data + i; \
        if (DOOR_EMPTY_ENTRY(entry)) { \
            continue; \
        } \
        int ex = entry->x + map->dx; \
        int ey = entry->y + map->dy; \
        int ez = entry->z + map->dz; \
        int ew = entry->w;

#define END_DOOR_MAP_FOR_EACH }

typedef struct {
    unsigned char x;
    unsigned char y;
    unsigned char z;
    signed char w;
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

void door_map_alloc(DoorMap *map, int dx, int dy, int dz, int mask);
void door_map_free(DoorMap *map);
void door_map_copy(DoorMap *dst, DoorMap *src);
void door_map_grow(DoorMap *map);
int door_map_set(DoorMap *map, int x, int y, int z, int w,
    int offset_into_gl_buffer, int face_count_in_gl_buffer, float ao[6][4],
    float light[6][4], int left, int right, int top,
    int bottom, int front, int back, float n, int shape,
    int extra, int transform);
DoorMapEntry *door_map_get(DoorMap *map, int x, int y, int z);

void make_door(
    float *data, float ao[6][4], float light[6][4],
    int left, int right, int top, int bottom, int front, int back,
    float x, float y, float z, float n, int w, int shape, int extra,
    int transform);

void door_toggle_open(DoorMap *door_map, DoorMapEntry *door, int x, int y,
    int z, GLuint buffer, size_t float_size);
