
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <GLES2/gl2.h>
#include "config.h"
#include "cube.h"
#include "door.h"
#include "item.h"
#include "pw.h"
#include "util.h"

void make_door_in_buffer_sub_data(int buffer, int float_size, DoorMapEntry *door);

int door_hash_int(int key) {
    key = ~key + (key << 15);
    key = key ^ (key >> 12);
    key = key + (key << 2);
    key = key ^ (key >> 4);
    key = key * 2057;
    key = key ^ (key >> 16);
    return key;
}

int door_hash(int x, int y, int z) {
    x = door_hash_int(x);
    y = door_hash_int(y);
    z = door_hash_int(z);
    return x ^ y ^ z;
}

void door_map_alloc(DoorMap *map, int dx, int dy, int dz, int mask) {
    map->dx = dx;
    map->dy = dy;
    map->dz = dz;
    map->mask = mask;
    map->size = 0;
    map->data = (DoorMapEntry *)calloc(map->mask + 1, sizeof(DoorMapEntry));
}

void door_map_free(DoorMap *map) {
    free(map->data);
}

void door_map_copy(DoorMap *dst, DoorMap *src) {
    dst->dx = src->dx;
    dst->dy = src->dy;
    dst->dz = src->dz;
    dst->mask = src->mask;
    dst->size = src->size;
    dst->data = (DoorMapEntry *)calloc(dst->mask + 1, sizeof(DoorMapEntry));
    memcpy(dst->data, src->data, (dst->mask + 1) * sizeof(DoorMapEntry));
}

static void door_entry_copy(DoorMapEntry *entry, int offset_into_gl_buffer,
    int face_count_in_gl_buffer, float ao[6][4], float light[6][4],
    int left, int right, int top, int bottom,
    int front, int back,
    float n, int shape, int extra, int transform) {
    entry->offset_into_gl_buffer = offset_into_gl_buffer;
    entry->face_count_in_gl_buffer = face_count_in_gl_buffer;
    memcpy(entry->ao, ao, sizeof(float) * 6 * 4);
    memcpy(entry->light, light, sizeof(float) * 6 * 4);
    entry->left = left;
    entry->right = right;
    entry->top = top;
    entry->bottom = bottom;
    entry->front = front;
    entry->back = back;
    entry->n = n;
    entry->shape = shape;
    entry->extra = extra;
    entry->transform = transform;
}

int door_map_set(DoorMap *map, int x, int y, int z, int w,
    int offset_into_gl_buffer, int face_count_in_gl_buffer,
    float ao[6][4], float light[6][4], int left, int right,
    int top, int bottom, int front, int back, float n,
    int shape, int extra, int transform) {
    unsigned int index = door_hash(x, y, z) & map->mask;
    x -= map->dx;
    y -= map->dy;
    z -= map->dz;
    DoorMapEntry *entry = map->data + index;
    int overwrite = 0;
    while (!DOOR_EMPTY_ENTRY(entry)) {
        if (entry->e.x == x && entry->e.y == y && entry->e.z == z) {
            overwrite = 1;
            break;
        }
        index = (index + 1) & map->mask;
        entry = map->data + index;
    }
    if (overwrite) {
        door_entry_copy(entry, offset_into_gl_buffer, face_count_in_gl_buffer,
            ao, light, left, right, top, bottom, front, back, n, shape, extra,
            transform);
        if (entry->e.w != w) {
            entry->e.w = w;
            return 1;
        }
    }
    else if (w) {
        entry->e.x = x;
        entry->e.y = y;
        entry->e.z = z;
        entry->e.w = w;
        door_entry_copy(entry, offset_into_gl_buffer, face_count_in_gl_buffer,
            ao, light, left, right, top, bottom, front, back, n, shape, extra,
            transform);
        map->size++;
        if (map->size * 2 > map->mask) {
            door_map_grow(map);
        }
        return 1;
    }
    return 0;
}

void door_map_clear(DoorMap *map, int x, int y, int z)
{
    unsigned int index = door_hash(x, y, z) & map->mask;
    DoorMapEntry *entry = map->data + index;
    if (x != map->dx + entry->e.x || y != map->dy + entry->e.y ||
        z != map->dz + entry->e.z) {
        return;
    }
    entry->e.w = 0;
    entry->shape = 0;
    entry->extra = 0;
    entry->transform = 0;
}

DoorMapEntry *door_map_get(DoorMap *map, int x, int y, int z) {
    unsigned int index = door_hash(x, y, z) & map->mask;
    x -= map->dx;
    y -= map->dy;
    z -= map->dz;
    if (x < 0 || x > 255) return 0;
    if (y < 0 || y > 255) return 0;
    if (z < 0 || z > 255) return 0;
    DoorMapEntry *entry = map->data + index;
    while (!DOOR_EMPTY_ENTRY(entry)) {
        if (entry->e.x == x && entry->e.y == y && entry->e.z == z) {
            return entry;
        }
        index = (index + 1) & map->mask;
        entry = map->data + index;
    }
    return 0;
}

void door_map_grow(DoorMap *map) {
    DoorMap new_map;
    new_map.dx = map->dx;
    new_map.dy = map->dy;
    new_map.dz = map->dz;
    new_map.mask = (map->mask << 1) | 1;
    new_map.size = 0;
    new_map.data = (DoorMapEntry *)calloc(new_map.mask + 1, sizeof(DoorMapEntry));
    DOOR_MAP_FOR_EACH(map, ex, ey, ez, ew) {
        door_map_set(&new_map, ex, ey, ez, ew,
            entry->offset_into_gl_buffer, entry->face_count_in_gl_buffer,
            entry->ao, entry->light, entry->left, entry->right, entry->top,
            entry->bottom, entry->front, entry->back, entry->n, entry->shape,
            entry->extra, entry->transform);
    } END_DOOR_MAP_FOR_EACH;
    free(map->data);
    map->mask = new_map.mask;
    map->size = new_map.size;
    map->data = new_map.data;
}

void make_door_in_buffer_sub_data(int buffer, int float_size,
    DoorMapEntry *door)
{
    // This is an optimisation to change the shape of just one door block.
    GLfloat door_data[6*10*6];  // 6 * components * faces
    make_door(
        door_data, door->ao, door->light, door->left, door->right, door->top,
        door->bottom, door->front, door->back, door->e.x, door->e.y, door->e.z,
        door->n, door->e.w, door->shape, door->extra, door->transform);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    if (config->use_hfloat) {
        hfloat hdata[6*10*6];
        for (size_t i=0; i<(6 * 10 * 6); i++) {
            hdata[i] = float_to_hfloat(door_data + i);
        }
        glBufferSubData(GL_ARRAY_BUFFER,
            door->offset_into_gl_buffer * float_size,
            6*10*door->face_count_in_gl_buffer*float_size, hdata);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER,
            door->offset_into_gl_buffer * float_size,
            6*10*door->face_count_in_gl_buffer*float_size, door_data);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

#define DOOR_POSITION_COUNT 4
void make_door_faces(
    float *data, float ao[6][4], float light[6][4],
    int left, int right, int top, int bottom, int front, int back,
    int wleft, int wright, int wtop, int wbottom, int wfront, int wback,
    float x, float y, float z, float n, int door_open, int transform)
{
    static const float positions[DOOR_POSITION_COUNT][6][4][3] = {
        {  // closed: +x, open: -z
        {{P14, P00, P00}, {P14, P00, P16}, {P14, P16, P00}, {P14, P16, P16}},  // -x
        {{P16, P00, P00}, {P16, P00, P16}, {P16, P16, P00}, {P16, P16, P16}},  // +x
        {{P14, P16, P00}, {P14, P16, P16}, {P16, P16, P00}, {P16, P16, P16}},  // +y
        {{P14, P00, P00}, {P14, P00, P16}, {P16, P00, P00}, {P16, P00, P16}},  // -y
        {{P14, P00, P00}, {P14, P16, P00}, {P16, P00, P00}, {P16, P16, P00}},  // -z
        {{P14, P00, P16}, {P14, P16, P16}, {P16, P00, P16}, {P16, P16, P16}}   // +z
        },
        {  // closed: -z, open: -x
        {{P00, P00, P00}, {P00, P00, P02}, {P00, P16, P00}, {P00, P16, P02}},  // -x
        {{P16, P00, P00}, {P16, P00, P02}, {P16, P16, P00}, {P16, P16, P02}},  // +x
        {{P00, P16, P00}, {P00, P16, P02}, {P16, P16, P00}, {P16, P16, P02}},  // +y
        {{P00, P00, P00}, {P00, P00, P02}, {P16, P00, P00}, {P16, P00, P02}},  // -y
        {{P00, P00, P00}, {P00, P16, P00}, {P16, P00, P00}, {P16, P16, P00}},  // -z
        {{P00, P00, P02}, {P00, P16, P02}, {P16, P00, P02}, {P16, P16, P02}}   // +z
        },
        {  // closed: +z, open: +x
        {{P00, P00, P14}, {P00, P00, P16}, {P00, P16, P14}, {P00, P16, P16}},  // -x
        {{P16, P00, P14}, {P16, P00, P16}, {P16, P16, P14}, {P16, P16, P16}},  // +x
        {{P00, P16, P14}, {P00, P16, P16}, {P16, P16, P14}, {P16, P16, P16}},  // +y
        {{P00, P00, P14}, {P00, P00, P16}, {P16, P00, P14}, {P16, P00, P16}},  // -y
        {{P00, P00, P14}, {P00, P16, P14}, {P16, P00, P14}, {P16, P16, P14}},  // -z
        {{P00, P00, P16}, {P00, P16, P16}, {P16, P00, P16}, {P16, P16, P16}}   // +z
        },
        {  // closed: -x, open: +z
        {{P00, P00, P00}, {P00, P00, P16}, {P00, P16, P00}, {P00, P16, P16}},  // -x
        {{P02, P00, P00}, {P02, P00, P16}, {P02, P16, P00}, {P02, P16, P16}},  // +x
        {{P00, P16, P00}, {P00, P16, P16}, {P02, P16, P00}, {P02, P16, P16}},  // +y
        {{P00, P00, P00}, {P00, P00, P16}, {P02, P00, P00}, {P02, P00, P16}},  // -y
        {{P00, P00, P00}, {P00, P16, P00}, {P02, P00, P00}, {P02, P16, P00}},  // -z
        {{P00, P00, P16}, {P00, P16, P16}, {P02, P00, P16}, {P02, P16, P16}}   // +z
        },
    };
    static const float normals[6][3] = {
        {-1, 0, 0},
        {+1, 0, 0},
        {0, +1, 0},
        {0, -1, 0},
        {0, 0, -1},
        {0, 0, +1}
    };
    static const float uvs[DOOR_POSITION_COUNT][6][4][2] =
    {
        {  // closed: +x, open: -z
        {{UV01, UV01}, {UV16, UV01}, {UV01, UV16}, {UV16, UV16}},
        {{UV01, UV01}, {UV16, UV01}, {UV01, UV16}, {UV16, UV16}},
        {{UV14, UV16}, {UV14, UV01}, {UV16, UV16}, {UV16, UV01}},
        {{UV14, UV01}, {UV14, UV16}, {UV16, UV01}, {UV16, UV16}},
        {{UV14, UV01}, {UV14, UV16}, {UV16, UV01}, {UV16, UV16}},
        {{UV16, UV01}, {UV16, UV16}, {UV14, UV01}, {UV14, UV16}}
        },
        {  // closed: +z, open: -x
        {{UV14, UV01}, {UV16, UV01}, {UV14, UV16}, {UV16, UV16}},
        {{UV16, UV01}, {UV14, UV01}, {UV16, UV16}, {UV14, UV16}},
        {{UV01, UV16}, {UV01, UV14}, {UV16, UV16}, {UV16, UV14}},
        {{UV01, UV14}, {UV01, UV16}, {UV16, UV14}, {UV16, UV16}},
        {{UV16, UV01}, {UV16, UV16}, {UV01, UV01}, {UV01, UV16}},
        {{UV16, UV01}, {UV16, UV16}, {UV01, UV01}, {UV01, UV16}}
        },
        {  // closed: -x, open: -z
        {{UV16, UV01}, {UV01, UV01}, {UV16, UV16}, {UV01, UV16}},
        {{UV16, UV01}, {UV01, UV01}, {UV16, UV16}, {UV01, UV16}},
        {{UV14, UV16}, {UV14, UV01}, {UV16, UV16}, {UV16, UV01}},
        {{UV14, UV01}, {UV14, UV16}, {UV16, UV01}, {UV16, UV16}},
        {{UV14, UV01}, {UV14, UV16}, {UV16, UV01}, {UV16, UV16}},
        {{UV16, UV01}, {UV16, UV16}, {UV14, UV01}, {UV14, UV16}}
        },
        {  // closed: -z, open: +x
        {{UV14, UV01}, {UV16, UV01}, {UV14, UV16}, {UV16, UV16}},
        {{UV16, UV01}, {UV14, UV01}, {UV16, UV16}, {UV14, UV16}},
        {{UV01, UV16}, {UV01, UV14}, {UV16, UV16}, {UV16, UV14}},
        {{UV01, UV14}, {UV01, UV16}, {UV16, UV14}, {UV16, UV16}},
        {{UV01, UV01}, {UV01, UV16}, {UV16, UV01}, {UV16, UV16}},
        {{UV01, UV01}, {UV01, UV16}, {UV16, UV01}, {UV16, UV16}},
        },
    };
    static const float indices[6][6] = {
        {0, 3, 2, 0, 1, 3},
        {0, 3, 1, 0, 2, 3},
        {0, 3, 2, 0, 1, 3},
        {0, 3, 1, 0, 2, 3},
        {0, 3, 2, 0, 1, 3},
        {0, 3, 1, 0, 2, 3}
    };
    static const float flipped[6][6] = {
        {0, 1, 2, 1, 3, 2},
        {0, 2, 1, 2, 3, 1},
        {0, 1, 2, 1, 3, 2},
        {0, 2, 1, 2, 3, 1},
        {0, 1, 2, 1, 3, 2},
        {0, 2, 1, 2, 3, 1}
    };
    static int transform_closed_map[DOOR_POSITION_COUNT][2] = {
        {DOOR_X_PLUS, DOOR_X_PLUS_FLIP},
        {DOOR_Z, DOOR_Z_FLIP},
        {DOOR_Z_PLUS, DOOR_Z_PLUS_FLIP},
        {DOOR_X, DOOR_X_FLIP},
    };
    static int transform_open_map[DOOR_POSITION_COUNT][2] = {
        {DOOR_Z, DOOR_Z_PLUS_FLIP},
        {DOOR_X, DOOR_X_PLUS_FLIP},
        {DOOR_X_PLUS, DOOR_X_FLIP},
        {DOOR_Z_PLUS, DOOR_Z_FLIP},
    };
    static int uvs_closed_map[DOOR_POSITION_COUNT][2] = {
        {DOOR_X_PLUS, DOOR_X_FLIP},
        {DOOR_Z_PLUS, DOOR_Z_FLIP},
        {DOOR_X, DOOR_X_PLUS_FLIP},
        {DOOR_Z, DOOR_Z_PLUS_FLIP},
    };
    static int uvs_open_map[DOOR_POSITION_COUNT][2] = {
        {DOOR_Z_PLUS, DOOR_Z_PLUS_FLIP},
        {DOOR_X, DOOR_X_FLIP},
        {DOOR_Z, DOOR_Z_FLIP},
        {DOOR_X_PLUS, DOOR_X_PLUS_FLIP},
    };
    float *d = data;
    float s = 0.0625;
    int faces[6] = {left, right, top, bottom, front, back};
    int tiles[6] = {wleft, wright, wtop, wbottom, wfront, wback};
    const float (*door_positions)[4][3];
    const float (*door_uvs)[4][2];
    int (*transform_map)[2];
    int (*uvs_map)[2];
    if (door_open) {
        transform_map = transform_open_map;
        uvs_map = uvs_open_map;
    } else {
        transform_map = transform_closed_map;
        uvs_map = uvs_closed_map;
    }
    int model_index = 0;
    for (int i=0; i<DOOR_POSITION_COUNT; i++) {
        if (transform_map[i][0] == transform || transform_map[i][1] == transform) {
            model_index = i;
            break;
        }
    }
    door_positions = positions[model_index];
    int uvs_index = 0;
    for (int i=0; i<DOOR_POSITION_COUNT; i++) {
        if (uvs_map[i][0] == transform || uvs_map[i][1] == transform) {
            uvs_index = i;
            break;
        }
    }
    door_uvs = uvs[uvs_index];

    for (int i = 0; i < 6; i++) {
        if (faces[i] == 0) {
            continue;
        }
        float du = (tiles[i] % 16) * s;
        float dv = (tiles[i] / 16) * s;
        int flip = ao[i][0] + ao[i][3] > ao[i][1] + ao[i][2];
        for (int v = 0; v < 6; v++) {
            int j = flip ? flipped[i][v] : indices[i][v];
            *(d++) = x + n * door_positions[i][j][0];
            *(d++) = y + n * door_positions[i][j][1];
            *(d++) = z + n * door_positions[i][j][2];
            *(d++) = normals[i][0];
            *(d++) = normals[i][1];
            *(d++) = normals[i][2];
            *(d++) = du + door_uvs[i][j][0];
            *(d++) = dv + door_uvs[i][j][1];
            *(d++) = ao[i][j];
            *(d++) = light[i][j];
        }
    }
}

void make_door(
    float *data, float ao[6][4], float light[6][4],
    int left, int right, int top, int bottom, int front, int back,
    float x, float y, float z, float n, int w, int shape, int extra,
    int transform)
{
    int wleft = blocks[w][0];
    int wright = blocks[w][1];
    int wtop = blocks[w][2];
    int wbottom = blocks[w][3];
    int wfront = blocks[w][4];
    int wback = blocks[w][5];

    int door_open = is_open(extra);

    // Map base texture to door texture (upper/lower)
    int upper_door;
    int lower_door;
    if (w == PLANK) {
        lower_door = 57;
        upper_door = 73;
    } else if (w == GLASS) {
        lower_door = 58;
        upper_door = 74;
    } else if (w >= COLOR_00 && w <= COLOR_15) {
        lower_door = blocks[w][1] - (16*5);
        upper_door = blocks[w][1] - (16*4);
    } else {
        lower_door = blocks[w][1] - (16*3);
        upper_door = blocks[w][1] - (16*2);
    }
    if (shape == LOWER_DOOR) {
        wleft = lower_door;
        wright = lower_door;
        wtop = lower_door;
        wbottom = lower_door;
        wfront = lower_door;
        wback = lower_door;
    } else { // UPPER_DOOR
        wleft = upper_door;
        wright = upper_door;
        wtop = upper_door;
        wbottom = upper_door;
        wfront = upper_door;
        wback = upper_door;
    }

    make_door_faces(
        data, ao, light,
        left, right, top, bottom, front, back,
        wleft, wright, wtop, wbottom, wfront, wback,
        x, y, z, n, door_open, transform);
}

void _door_toggle_open(DoorMapEntry *door, int x, int y, int z, GLuint buffer,
    size_t float_size)
{
    if (is_open(door->extra)) {
        door->extra &= ~EXTRA_BIT_OPEN;
    } else {
        door->extra |= EXTRA_BIT_OPEN;
    }
    set_extra_non_dirty(x, y, z, door->extra);
    make_door_in_buffer_sub_data(buffer, float_size, door);
}

void door_toggle_open(DoorMap *door_map, DoorMapEntry *door, int x, int y,
    int z, GLuint buffer, size_t float_size)
{
    _door_toggle_open(door, x, y, z, buffer, float_size);

    DoorMapEntry *matching_door = NULL;
    if (door->shape == UPPER_DOOR) {
        DoorMapEntry *e = door_map_get(door_map, x, y - 1, z);
        if (e && e->shape == LOWER_DOOR) {
            matching_door = e;
        }
    } else if (door->shape == LOWER_DOOR) {
        DoorMapEntry *e = door_map_get(door_map, x, y + 1, z);
        if (e && e->shape == UPPER_DOOR) {
            matching_door = e;
        }
    }
    if (matching_door) {
        _door_toggle_open(matching_door, x, matching_door->e.y, z, buffer,
            float_size);
    }
}
