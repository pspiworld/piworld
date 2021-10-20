#include <math.h>
#include <string.h>
#include "chunks.h"
#include "client.h"
#include "clients.h"
#include "db.h"
#include "item.h"
#include "local_player.h"
#include "local_players.h"
#include "player.h"
#include "pw.h"

Chunk chunks[MAX_CHUNKS];
int chunk_count;

int get_shape(int x, int y, int z);

int has_lights(Chunk *chunk);

void chunks_reset(void)
{
    memset(chunks, 0, sizeof(Chunk) * MAX_CHUNKS);
    chunk_count = 0;
}

Chunk *find_chunk(int p, int q)
{
    for (int i = 0; i < chunk_count; i++) {
        Chunk *chunk = chunks + i;
        if (chunk->p == p && chunk->q == q) {
            return chunk;
        }
    }
    return 0;
}

void dirty_chunk(Chunk *chunk)
{
    chunk->dirty = 1;
    chunk->dirty_signs = 1;
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

int highest_block(float x, float z)
{
    int result = -1;
    int nx = roundf(x);
    int nz = roundf(z);
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->map;
        MAP_FOR_EACH(map, ex, ey, ez, ew) {
            if (is_obstacle(ew, 0, 0) && ex == nx && ez == nz) {
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
    for (int i = 0; i < chunk_count; i++) {
        Chunk *chunk = chunks + i;
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

int hit_test_face(Player *player, int *x, int *y, int *z, int *face)
{
    State *s = &player->state;
    int w = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, x, y, z);
    if (is_obstacle(w, 0, 0)) {
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

int collide(int height, float *x, float *y, float *z, float *ydiff)
{
    #define AUTO_JUMP_LIMIT 0.5
    int result = 0;
    int p = chunked(*x);
    int q = chunked(*z);
    Chunk *chunk = find_chunk(p, q);
    if (!chunk) {
        return result;
    }
    Map *map = &chunk->map;
    Map *shape_map = &chunk->shape;
    Map *extra_map = &chunk->extra;
    int nx = roundf(*x);
    int ny = roundf(*y);
    int nz = roundf(*z);
    float px = *x - nx;
    float py = *y - ny;
    float pz = *z - nz;
    float pad = 0.25;
    uint8_t coll_ok = 1;
    uint8_t need_jump = 0;
    for (int dy = 0; dy < height; dy++) {
        if (px < -pad && is_obstacle(map_get(map, nx - 1, ny - dy, nz),
                                     map_get(shape_map, nx - 1, ny - dy, nz),
                                     map_get(extra_map, nx - 1, ny - dy, nz))) {
            *x = nx - pad;
            if (dy == 0) {
                coll_ok = 0;
            } else if (coll_ok &&
                       item_height(map_get(shape_map, nx - 1, ny - dy, nz)) <= AUTO_JUMP_LIMIT) {
                need_jump = 1;
            }
        }
        if (px > pad && is_obstacle(map_get(map, nx + 1, ny - dy, nz),
                                    map_get(shape_map, nx + 1, ny - dy, nz),
                                    map_get(extra_map, nx + 1, ny - dy, nz))) {
            *x = nx + pad;
            if (dy == 0) {
                coll_ok = 0;
            } else if (coll_ok &&
                       item_height(map_get(shape_map, nx + 1, ny - dy, nz)) <= AUTO_JUMP_LIMIT) {
                need_jump = 1;
            }
        }
        if (py < -pad && is_obstacle(map_get(map, nx, ny - dy - 1, nz),
                                     map_get(shape_map, nx, ny - dy - 1, nz),
                                     map_get(extra_map, nx, ny - dy - 1, nz))) {
            *y = ny - pad;
            result = 1;
        }
        if (py > pad && is_obstacle(map_get(map, nx, ny - dy + 1, nz),
                                    map_get(shape_map, nx, ny - dy + 1, nz),
                                    map_get(extra_map, nx, ny - dy + 1, nz))) {
            // reached when player jumps and hits their head on block above
            *y = ny + pad;
            result = 1;
        }
        if (pz < -pad && is_obstacle(map_get(map, nx, ny - dy, nz - 1),
                                     map_get(shape_map, nx, ny - dy, nz - 1),
                                     map_get(extra_map, nx, ny - dy, nz - 1))) {
            *z = nz - pad;
            if (dy == 0) {
                coll_ok = 0;
            } else if (coll_ok &&
                       item_height(map_get(shape_map, nx, ny - dy, nz - 1)) <= AUTO_JUMP_LIMIT) {
                need_jump = 1;
            }
        }
        if (pz > pad && is_obstacle(map_get(map, nx, ny - dy, nz + 1),
                                    map_get(shape_map, nx, ny - dy, nz + 1),
                                    map_get(extra_map, nx, ny - dy, nz + 1))) {
            *z = nz + pad;
            if (dy == 0) {
                coll_ok = 0;
            } else if (coll_ok &&
                       item_height(map_get(shape_map, nx, ny - dy, nz + 1)) <= AUTO_JUMP_LIMIT) {
                need_jump = 1;
            }
        }

        // check the 4 diagonally neighboring blocks for obstacle as well
        if (px < -pad && pz > pad &&
            is_obstacle(map_get(map, nx - 1, ny - dy, nz + 1),
                        map_get(shape_map, nx - 1, ny - dy, nz + 1),
                        map_get(extra_map, nx - 1, ny - dy, nz + 1))) {
            if(ABS(px) < ABS(pz)) {
                *x = nx - pad;
            } else {
                *z = nz + pad;
            }
        }
        if (px > pad && pz > pad &&
            is_obstacle(map_get(map, nx + 1, ny - dy, nz + 1),
                        map_get(shape_map, nx + 1, ny - dy, nz + 1),
                        map_get(extra_map, nx + 1, ny - dy, nz + 1))) {
            if(ABS(px) < ABS(pz)) {
                *x = nx + pad;
            } else {
                *z = nz + pad;
            }
        }
        if (px < -pad && pz < -pad &&
            is_obstacle(map_get(map, nx - 1, ny - dy, nz - 1),
                        map_get(shape_map, nx - 1, ny - dy, nz - 1),
                        map_get(extra_map, nx - 1, ny - dy, nz - 1))) {
            if(ABS(px) < ABS(pz)) {
                *x = nx - pad;
            } else {
                *z = nz - pad;
            }
        }
        if (px > pad && pz < -pad &&
            is_obstacle(map_get(map, nx + 1, ny - dy, nz - 1),
                        map_get(shape_map, nx + 1, ny - dy, nz - 1),
                        map_get(extra_map, nx + 1, ny - dy, nz - 1))) {
            if(ABS(px) < ABS(pz)) {
                *x = nx + pad;
            } else {
                *z = nz - pad;
            }
        }
    }
    /* If there's no block at head height, a block at foot height, and
       standing on firm ground, then autojump */
    if (need_jump && result == 1) {
        *ydiff = 8;
        result = 0;
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

int has_lights(Chunk *chunk)
{
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

void delete_chunks(int delete_radius)
{
    int count = chunk_count;
    int states_count = 0;
    // Maximum states include the basic player view and 2 observe views.
    #define MAX_STATES (MAX_LOCAL_PLAYERS * 3)
    State *states[MAX_STATES];

    for (int p = 0; p < MAX_LOCAL_PLAYERS; p++) {
        LocalPlayer *local = get_local_player(p);
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
        Chunk *chunk = chunks + i;
        int delete = 1;
        for (int j = 0; j < states_count; j++) {
            State *s = states[j];
            int p = chunked(s->x);
            int q = chunked(s->z);
            if (chunk_distance(chunk, p, q) < delete_radius) {
                delete = 0;
                break;
            }
        }
        if (delete) {
            map_free(&chunk->map);
            map_free(&chunk->extra);
            map_free(&chunk->lights);
            map_free(&chunk->shape);
            map_free(&chunk->transform);
            sign_list_free(&chunk->signs);
            door_map_free(&chunk->doors);
            del_buffer(chunk->buffer);
            del_buffer(chunk->sign_buffer);
            Chunk *other = chunks + (--count);
            memcpy(chunk, other, sizeof(Chunk));
        }
    }
    chunk_count = count;
}

void delete_all_chunks(void)
{
    for (int i = 0; i < chunk_count; i++) {
        Chunk *chunk = chunks + i;
        map_free(&chunk->map);
        map_free(&chunk->extra);
        map_free(&chunk->lights);
        map_free(&chunk->shape);
        map_free(&chunk->transform);
        door_map_free(&chunk->doors);
        sign_list_free(&chunk->signs);
        del_buffer(chunk->buffer);
        del_buffer(chunk->sign_buffer);
    }
    chunk_count = 0;
}

Chunk *next_available_chunk(void)
{
    Chunk *chunk = NULL;
    if (chunk_count < MAX_CHUNKS) {
        chunk = chunks + chunk_count++;
    }
    return chunk;
}

void unset_sign(int x, int y, int z)
{
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        SignList *signs = &chunk->signs;
        if (sign_list_remove_all(signs, x, y, z)) {
            chunk->dirty_signs = 1;
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
            chunk->dirty_signs = 1;
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
            chunk->dirty_signs = 1;
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

const unsigned char *get_sign(int p, int q, int x, int y, int z, int face)
{
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        SignList *signs = &chunk->signs;
        for (size_t i = 0; i < signs->size; i++) {
            Sign *e = signs->data + i;
            if (e->x == x && e->y == y && e->z == z && e->face == face) {
                return (const unsigned char *)e->text;
            }
        }
    } else {
        // TODO: support server
        return db_get_sign(p, q, x, y, z, face);
    }
    return NULL;
}

void toggle_light(int x, int y, int z)
{
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

int _set_light(int p, int q, int x, int y, int z, int w)
{
    Chunk *chunk = find_chunk(p, q);
    if (w < 0) {
        w = 0;
    }
    if (w > 15) {
        w = 15;
    }
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
    return w;
}

void set_light(int p, int q, int x, int y, int z, int w)
{
    w = _set_light(p, q, x, y, z, w);
    client_light(x, y, z, w);
}

int get_light(int p, int q, int x, int y, int z)
{
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->lights;
        return map_get(map, x, y, z);
    } else {
        // TODO: support server
        chunk = next_available_chunk();
        if (chunk) {
            create_chunk(chunk, p, q);
            return map_get(&chunk->lights, x, y, z);
        }
    }
    return 0;
}

void set_extra(int x, int y, int z, int w)
{
    int p = chunked(x);
    int q = chunked(z);
    _set_extra(p, q, x, y, z, w, 1);
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
            _set_extra(p + dx, q + dz, x, y, z, -w, 1);
        }
    }
    client_extra(x, y, z, w);
}

void set_extra_non_dirty(int x, int y, int z, int w)
{
    int p = chunked(x);
    int q = chunked(z);
    _set_extra(p, q, x, y, z, w, 0);
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
            _set_extra(p + dx, q + dz, x, y, z, -w, 0);
        }
    }
    client_extra(x, y, z, w);
}

void _set_extra(int p, int q, int x, int y, int z, int w, int dirty)
{
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->extra;
        if (map_set(map, x, y, z, w)) {
            if (dirty) {
                dirty_chunk(chunk);
            }
            db_insert_extra(p, q, x, y, z, w);
        }
    }
    else {
        db_insert_extra(p, q, x, y, z, w);
    }
}

int get_extra(int x, int y, int z)
{
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->extra;
        return map_get(map, x, y, z);
    } else {
        // TODO: support server
        chunk = next_available_chunk();
        if (chunk) {
            create_chunk(chunk, p, q);
            return map_get(&chunk->extra, x, y, z);
        }
    }
    return 0;
}

void set_shape(int x, int y, int z, int w)
{
    int p = chunked(x);
    int q = chunked(z);
    _set_shape(p, q, x, y, z, w, 1);
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
            _set_shape(p + dx, q + dz, x, y, z, -w, 1);
        }
    }
    client_shape(x, y, z, w);
}

void _set_shape(int p, int q, int x, int y, int z, int w, int dirty)
{
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->shape;
        if (map_set(map, x, y, z, w)) {
            if (dirty) {
                dirty_chunk(chunk);
            }
            db_insert_shape(p, q, x, y, z, w);
        }
    }
    else {
        db_insert_shape(p, q, x, y, z, w);
    }
}

int get_shape(int x, int y, int z)
{
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->shape;
        return map_get(map, x, y, z);
    } else {
        // TODO: support server
        chunk = next_available_chunk();
        if (chunk) {
            create_chunk(chunk, p, q);
            return map_get(&chunk->shape, x, y, z);
        }
    }
    return 0;
}

void set_transform(int x, int y, int z, int w)
{
    int p = chunked(x);
    int q = chunked(z);
    _set_transform(p, q, x, y, z, w, 1);
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
            _set_transform(p + dx, q + dz, x, y, z, -w, 1);
        }
    }
    client_transform(x, y, z, w);
}

void _set_transform(int p, int q, int x, int y, int z, int w, int dirty)
{
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->transform;
        if (map_set(map, x, y, z, w)) {
            if (dirty) {
                dirty_chunk(chunk);
            }
            db_insert_transform(p, q, x, y, z, w);
        }
    }
    else {
        db_insert_transform(p, q, x, y, z, w);
    }
}

int get_transform(int x, int y, int z)
{
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->transform;
        return map_get(map, x, y, z);
    } else {
        // TODO: support server
        chunk = next_available_chunk();
        if (chunk) {
            create_chunk(chunk, p, q);
            return map_get(&chunk->transform, x, y, z);
        }
    }
    return 0;
}

void _set_block(int p, int q, int x, int y, int z, int w, int dirty)
{
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
        _set_light(p, q, x, y, z, 0);
        _set_extra(p, q, x, y, z, 0, 1);
        _set_shape(p, q, x, y, z, 0, 1);
        _set_transform(p, q, x, y, z, 0, 1);
        door_map_clear(&chunk->doors, x, y, z);
    }
}

void set_block(int x, int y, int z, int w)
{
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

int get_block(int x, int y, int z)
{
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->map;
        return map_get(map, x, y, z);
    } else {
        // TODO: support server
        chunk = next_available_chunk();
        if (chunk) {
            create_chunk(chunk, p, q);
            return map_get(&chunk->map, x, y, z);
        }
    }
    return 0;
}

void benchmark_chunks(int count)
{
    for (int i=0; i<count; i++) {
        Chunk *chunk = chunks + i;
        create_chunk(chunk, i, i);
    }
}
