#include <math.h>
#include <stdlib.h>
#include "chunk.h"
#include "chunks.h"
#include "client.h"
#include "config.h"
#include "cube.h"
#include "db.h"
#include "fence.h"
#include "item.h"
#include "matrix.h"
#include "noise.h"
#include "pw.h"
#include "pwlua_worldgen.h"
#include "util.h"
#include "world.h"

void init_chunk(Chunk *chunk, int p, int q);

int chunked(float x)
{
    return floorf(roundf(x) / CHUNK_SIZE);
}

int chunk_distance(Chunk *chunk, int p, int q)
{
    int dp = ABS(chunk->p - p);
    int dq = ABS(chunk->q - q);
    return MAX(dp, dq);
}

int chunk_visible(float planes[6][4], int p, int q, int miny, int maxy,
    int ortho)
{
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
    int n = ortho ? 4 : 6;
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

void load_chunk(WorkerItem *item, lua_State *L)
{
    int p = item->p;
    int q = item->q;
    Map *block_map = item->block_maps[1][1];
    Map *extra_map = item->extra_maps[1][1];
    Map *light_map = item->light_maps[1][1];
    Map *shape_map = item->shape_maps[1][1];
    Map *transform_map = item->transform_maps[1][1];
    SignList *signs = &item->signs;
    sign_list_alloc(signs, 16);
    if (L != NULL) {
        pwlua_worldgen(L, p, q, block_map, extra_map, light_map, shape_map,
                       signs, transform_map);
    } else {
        create_world(p, q, map_set_func, block_map);
    }
    db_load_blocks(block_map, p, q);
    db_load_extras(extra_map, p, q);
    db_load_lights(light_map, p, q);
    db_load_shapes(shape_map, p, q);
    db_load_signs(signs, p, q);
    db_load_transforms(transform_map, p, q);
}

void request_chunk(int p, int q)
{
    int key = db_get_key(p, q);
    client_chunk(p, q, key);
}

void init_chunk(Chunk *chunk, int p, int q)
{
    chunk->p = p;
    chunk->q = q;
    chunk->faces = 0;
    chunk->sign_faces = 0;
    chunk->buffer = 0;
    chunk->sign_buffer = 0;
    dirty_chunk(chunk);
    SignList *signs = &chunk->signs;
    sign_list_alloc(signs, 16);
    Map *block_map = &chunk->map;
    Map *extra_map = &chunk->extra;
    Map *light_map = &chunk->lights;
    Map *shape_map = &chunk->shape;
    Map *transform_map = &chunk->transform;
    DoorMap *doors_map = &chunk->doors;
    int dx = p * CHUNK_SIZE - 1;
    int dy = 0;
    int dz = q * CHUNK_SIZE - 1;
    map_alloc(block_map, dx, dy, dz, 0x3fff);
    map_alloc(extra_map, dx, dy, dz, 0xf);
    map_alloc(light_map, dx, dy, dz, 0xf);
    map_alloc(shape_map, dx, dy, dz, 0xf);
    map_alloc(transform_map, dx, dy, dz, 0xf);
    door_map_alloc(doors_map, dx, dy, dz, 0xf);
}

void create_chunk(Chunk *chunk, int p, int q)
{
    init_chunk(chunk, p, q);

    WorkerItem _item;
    WorkerItem *item = &_item;
    item->p = chunk->p;
    item->q = chunk->q;
    item->block_maps[1][1] = &chunk->map;
    item->extra_maps[1][1] = &chunk->extra;
    item->light_maps[1][1] = &chunk->lights;
    item->shape_maps[1][1] = &chunk->shape;
    item->transform_maps[1][1] = &chunk->transform;
    item->door_maps[1][1] = &chunk->doors;
    load_chunk(item, pwlua_worldgen_get_main_thread_instance());
    sign_list_free(&chunk->signs);
    sign_list_copy(&chunk->signs, &item->signs);
    sign_list_free(&item->signs);

    request_chunk(p, q);
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

void compute_chunk(WorkerItem *item)
{
    char *opaque = (char *)calloc(XZ_SIZE * XZ_SIZE * Y_SIZE, sizeof(char));
    char *light = (char *)calloc(XZ_SIZE * XZ_SIZE * Y_SIZE, sizeof(char));
    char *highest = (char *)calloc(XZ_SIZE * XZ_SIZE, sizeof(char));

    int ox = item->p * CHUNK_SIZE - CHUNK_SIZE - 1;
    int oy = -1;
    int oz = item->q * CHUNK_SIZE - CHUNK_SIZE - 1;

    // check for shapes
    Map *shape_map = item->shape_maps[1][1];
    int has_shape = 0;
    for (int a = 0; a < 3; a++) {
        for (int b = 0; b < 3; b++) {
            Map *map = item->shape_maps[a][b];
            if (map && map->size) {
                has_shape = 1;
            }
        }
    }

    // check for extra
    Map *extra_map = item->extra_maps[1][1];
    int has_extra = 0;
    for (int a = 0; a < 3; a++) {
        for (int b = 0; b < 3; b++) {
            Map *map = item->extra_maps[a][b];
            if (map && map->size) {
                has_extra = 1;
            }
        }
    }

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
            if (has_shape) {
                shape_map = item->shape_maps[a][b];
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
                if (has_shape && shape_map && map_get(shape_map, ex, ey, ez)) {
                    opaque[XYZ(x, y, z)] = 0;
                }
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
    if (has_shape) {
        shape_map = item->shape_maps[1][1];
    }
    int has_transform = 0;
    Map *transform_map = item->transform_maps[1][1];
    if (transform_map && transform_map->size) {
        has_transform = 1;
    }
    DoorMap *door_map = item->door_maps[1][1];

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
        } else if (has_shape && shape_map) {
            int shape = map_get(shape_map, ex, ey, ez);
            if (shape) {
                if (shape >= SLAB1 && shape <= SLAB15) {
                    // Top face of slab is viewable when a block is above the slab.
                    f3 = 1;
                    total = f1 + f2 + f3 + f4 + f5 + f6;
                } else if (shape == UPPER_DOOR || shape == LOWER_DOOR) {
                    // Different side faces of a door may be visible when open.
                    f1 = 1;
                    f2 = 1;
                    f5 = 1;
                    f6 = 1;
                    total = f1 + f2 + f3 + f4 + f5 + f6;
                } else if (shape >= FENCE && shape <= GATE) {
                    // Hidden face removal not yet enabled for fence shapes.
                    total = fence_face_count(shape);
                }
            }
        }
        miny = MIN(miny, ey);
        maxy = MAX(maxy, ey);
        faces += total;
    } END_MAP_FOR_EACH;

    // generate geometry
    GLfloat *data = malloc_faces(10, faces, sizeof(GLfloat));
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
        if (has_shape && shape_map) {
            int shape = map_get(shape_map, ex, ey, ez);
            if (shape >= SLAB1 && shape <= SLAB15) {
                // Top face of slab is viewable when a block is above the slab.
                f3 = 1;
                total = f1 + f2 + f3 + f4 + f5 + f6;
            } else if (shape == UPPER_DOOR || shape == LOWER_DOOR) {
                // Different side faces of a door may be visible when open.
                f1 = 1;
                f2 = 1;
                f5 = 1;
                f6 = 1;
                total = f1 + f2 + f3 + f4 + f5 + f6;
            } else if (shape >= FENCE && shape <= GATE) {
                f1 = 1; f2 = 1; f3 = 1; f4 = 1; f5 = 1; f6 = 1;
                total = fence_face_count(shape);
            }
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
                entry->e.x, entry->e.y, entry->e.z, 0.5, ew, rotation);
        }
        else if (has_shape && shape_map) {
            int shape = map_get(shape_map, ex, ey, ez);
            if (shape) {
                int transform = 0;
                if (has_transform) {
                    transform = map_get(transform_map, ex, ey, ez);
                }
                if (shape >= SLAB1 && shape <= SLAB15) {
                    make_slab(
                        data + offset, ao, light,
                        f1, f2, f3, f4, f5, f6,
                        entry->e.x, entry->e.y, entry->e.z, 0.5, ew, shape);
                } else if (shape == LOWER_DOOR || shape == UPPER_DOOR) {
                    int extra = 0;
                    if (has_extra && extra_map)  {
                        extra = map_get(extra_map, ex, ey, ez);
                    }
                    door_map_set(door_map, ex, ey, ez, ew, offset, total, ao,
                        light, f1, f2, f3, f4, f5, f6, 0.5, shape, extra,
                        transform);
                    make_door(
                        data + offset, ao, light,
                        f1, f2, f3, f4, f5, f6,
                        entry->e.x, entry->e.y, entry->e.z, 0.5, ew, shape,
                        extra, transform);
                } else if (shape >= FENCE && shape <= GATE) {
                    int extra = 0;
                    if (has_extra && extra_map)  {
                        extra = map_get(extra_map, ex, ey, ez);
                    }
                    if (shape == GATE) {
                        door_map_set(door_map, ex, ey, ez, ew, offset, total, ao,
                            light, f1, f2, f3, f4, f5, f6, 0.5, shape, extra,
                            transform);
                    }
                    make_fence(data + offset, ao, light,
                        f1, f2, f3, f4, f5, f6,
                        entry->e.x, entry->e.y, entry->e.z, 0.5, ew, shape,
                        extra, transform);
                }

            } else {
                make_cube(
                    data + offset, ao, light,
                    f1, f2, f3, f4, f5, f6,
                    entry->e.x, entry->e.y, entry->e.z, 0.5, ew);
            }
        }
        else {
            make_cube(
                data + offset, ao, light,
                f1, f2, f3, f4, f5, f6,
                entry->e.x, entry->e.y, entry->e.z, 0.5, ew);
        }
        offset += total * 60;
    } END_MAP_FOR_EACH;

    free(opaque);
    free(light);
    free(highest);

    item->miny = miny;
    item->maxy = maxy;
    item->faces = faces;
    if (config->use_hfloat) {
        hfloat *hdata = malloc_faces(10, faces, sizeof(hfloat));
        for (int i=0; i < (6 * 10 * faces); i++) {
            hdata[i] = float_to_hfloat(data + i);
        }
        free(data);
        item->data = hdata;
    } else {
        item->data = data;
    }
}

void generate_chunk(Chunk *chunk, WorkerItem *item, size_t float_size)
{
    chunk->miny = item->miny;
    chunk->maxy = item->maxy;
    chunk->faces = item->faces;
    del_buffer(chunk->buffer);
    chunk->buffer = gen_faces(10, item->faces, item->data, float_size);
    gen_sign_chunk_buffer(chunk);
}

void ensure_chunks_worker(Player *player, Worker *worker, int width,
    int height, int fov, int ortho, int render_radius, int create_radius)
{
    State *s = &player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, width, height,
        s->x, s->y, s->z, s->rx, s->ry, fov, ortho, render_radius);
    float planes[6][4];
    frustum_planes(planes, render_radius, matrix);
    int p = chunked(s->x);
    int q = chunked(s->z);
    int r = create_radius;
    int start = 0x0fffffff;
    int best_score = start;
    int best_a = 0;
    int best_b = 0;
    for (int dp = -r; dp <= r; dp++) {
        for (int dq = -r; dq <= r; dq++) {
            int a = p + dp;
            int b = q + dq;
            int index = (ABS(a) ^ ABS(b)) % config->worker_count;
            if (index != worker->index) {
                continue;
            }
            Chunk *chunk = find_chunk(a, b);
            if (chunk && !chunk->dirty) {
                continue;
            }
            int distance = MAX(ABS(dp), ABS(dq));
            int invisible = !chunk_visible(planes, a, b, 0, 256, ortho);
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
        chunk = next_available_chunk();
        if (chunk) {
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
                Map *extra_map = malloc(sizeof(Map));
                map_copy(extra_map, &other->extra);
                Map *light_map = malloc(sizeof(Map));
                map_copy(light_map, &other->lights);
                Map *shape_map = malloc(sizeof(Map));
                map_copy(shape_map, &other->shape);
                Map *transform_map = malloc(sizeof(Map));
                map_copy(transform_map, &other->transform);
                DoorMap *door_map = malloc(sizeof(DoorMap));
                door_map_copy(door_map, &other->doors);
                item->block_maps[dp + 1][dq + 1] = block_map;
                item->extra_maps[dp + 1][dq + 1] = extra_map;
                item->light_maps[dp + 1][dq + 1] = light_map;
                item->shape_maps[dp + 1][dq + 1] = shape_map;
                item->transform_maps[dp + 1][dq + 1] = transform_map;
                item->door_maps[dp + 1][dq + 1] = door_map;
            }
            else {
                item->block_maps[dp + 1][dq + 1] = 0;
                item->extra_maps[dp + 1][dq + 1] = 0;
                item->light_maps[dp + 1][dq + 1] = 0;
                item->shape_maps[dp + 1][dq + 1] = 0;
                item->transform_maps[dp + 1][dq + 1] = 0;
                item->door_maps[dp + 1][dq + 1] = 0;
            }
        }
    }
    chunk->dirty = 0;
    worker->state = WORKER_BUSY;
    cnd_signal(&worker->cnd);
}

void gen_chunk_buffer(Chunk *chunk, size_t float_size)
{
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
                item->extra_maps[dp + 1][dq + 1] = &other->extra;
                item->light_maps[dp + 1][dq + 1] = &other->lights;
                item->shape_maps[dp + 1][dq + 1] = &other->shape;
                item->transform_maps[dp + 1][dq + 1] = &other->transform;
                item->door_maps[dp + 1][dq + 1] = &other->doors;
            }
            else {
                item->block_maps[dp + 1][dq + 1] = 0;
                item->extra_maps[dp + 1][dq + 1] = 0;
                item->light_maps[dp + 1][dq + 1] = 0;
                item->shape_maps[dp + 1][dq + 1] = 0;
                item->transform_maps[dp + 1][dq + 1] = 0;
                item->door_maps[dp + 1][dq + 1] = 0;
            }
        }
    }
    compute_chunk(item);
    generate_chunk(chunk, item, float_size);
    chunk->dirty = 0;
}

void force_chunks(Player *player, size_t float_size)
{
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
                    gen_chunk_buffer(chunk, float_size);
                }
                if (chunk->dirty_signs) {
                    gen_sign_chunk_buffer(chunk);
                }
            } else {
                chunk = next_available_chunk();
                if (chunk) {
                    create_chunk(chunk, a, b);
                    gen_chunk_buffer(chunk, float_size);
                }
            }
        }
    }
}

