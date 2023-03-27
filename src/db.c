#include <stdio.h>
#include <string.h>
#include "config.h"
#include "db.h"
#include "ring.h"
#include "sqlite3.h"
#include "tinycthread.h"

static int db_enabled = 0;

static sqlite3 *db;
static sqlite3_stmt *insert_block_stmt;
static sqlite3_stmt *insert_extra_stmt;
static sqlite3_stmt *insert_light_stmt;
static sqlite3_stmt *insert_shape_stmt;
static sqlite3_stmt *insert_transform_stmt;
static sqlite3_stmt *insert_sign_stmt;
static sqlite3_stmt *delete_sign_stmt;
static sqlite3_stmt *delete_signs_stmt;
static sqlite3_stmt *load_blocks_stmt;
static sqlite3_stmt *load_extras_stmt;
static sqlite3_stmt *load_lights_stmt;
static sqlite3_stmt *load_shapes_stmt;
static sqlite3_stmt *load_transforms_stmt;
static sqlite3_stmt *load_signs_stmt;
static sqlite3_stmt *get_sign_stmt;
static sqlite3_stmt *get_light_stmt;
static sqlite3_stmt *get_key_stmt;
static sqlite3_stmt *set_key_stmt;
static sqlite3_stmt *get_option_stmt;
static sqlite3_stmt *set_option_stmt;

static Ring ring;
static thrd_t thrd;
static mtx_t mtx;
static cnd_t cnd;
static mtx_t load_mtx;

void db_enable(void) {
    db_enabled = 1;
}

void db_disable(void) {
    db_enabled = 0;
}

int get_db_enabled(void) {
    return db_enabled;
}

int db_init(char *path) {
    if (!db_enabled) {
        return 0;
    }
    if (config->verbose) {
        printf("\nLoading db from: %s\n", path);
    }
    static const char *create_query =
        "create table if not exists state ("
        "   x float not null,"
        "   y float not null,"
        "   z float not null,"
        "   rx float not null,"
        "   ry float not null"
        ");"
        "create table if not exists player_name ("
        "    name text not null"
        ");"
        "create table if not exists block ("
        "    p int not null,"
        "    q int not null,"
        "    x int not null,"
        "    y int not null,"
        "    z int not null,"
        "    w int not null"
        ");"
        "create table if not exists extra ("
        "    p int not null,"
        "    q int not null,"
        "    x int not null,"
        "    y int not null,"
        "    z int not null,"
        "    w int not null"
        ");"
        "create table if not exists light ("
        "    p int not null,"
        "    q int not null,"
        "    x int not null,"
        "    y int not null,"
        "    z int not null,"
        "    w int not null"
        ");"
        "create table if not exists key ("
        "    p int not null,"
        "    q int not null,"
        "    key int not null"
        ");"
        "create table if not exists shape ("
        "    p int not null,"
        "    q int not null,"
        "    x int not null,"
        "    y int not null,"
        "    z int not null,"
        "    w int not null"
        ");"
        "create table if not exists transform ("
        "    p int not null,"
        "    q int not null,"
        "    x int not null,"
        "    y int not null,"
        "    z int not null,"
        "    w int not null"
        ");"
        "create table if not exists sign ("
        "    p int not null,"
        "    q int not null,"
        "    x int not null,"
        "    y int not null,"
        "    z int not null,"
        "    face int not null,"
        "    text text not null"
        ");"
        "create table if not exists option ("
        "    name text not null,"
        "    value text not null"
        ");"
        "create unique index if not exists block_pqxyz_idx on block (p, q, x, y, z);"
        "create unique index if not exists extra_pqxyz_idx on extra (p, q, x, y, z);"
        "create unique index if not exists light_pqxyz_idx on light (p, q, x, y, z);"
        "create unique index if not exists key_pq_idx on key (p, q);"
        "create unique index if not exists option_idx on option (name);"
        "create unique index if not exists shape_pqxyz_idx on shape (p, q, x, y, z);"
        "create unique index if not exists transform_pqxyz_idx on transform (p, q, x, y, z);"
        "create unique index if not exists sign_xyzface_idx on sign (x, y, z, face);"
        "create index if not exists sign_pq_idx on sign (p, q);";
    static const char *insert_block_query =
        "insert or replace into block (p, q, x, y, z, w) "
        "values (?, ?, ?, ?, ?, ?);";
    static const char *insert_extra_query =
        "insert or replace into extra (p, q, x, y, z, w) "
        "values (?, ?, ?, ?, ?, ?);";
    static const char *insert_light_query =
        "insert or replace into light (p, q, x, y, z, w) "
        "values (?, ?, ?, ?, ?, ?);";
    static const char *insert_shape_query =
        "insert or replace into shape (p, q, x, y, z, w) "
        "values (?, ?, ?, ?, ?, ?);";
    static const char *insert_transform_query =
        "insert or replace into transform (p, q, x, y, z, w) "
        "values (?, ?, ?, ?, ?, ?);";
    static const char *insert_sign_query =
        "insert or replace into sign (p, q, x, y, z, face, text) "
        "values (?, ?, ?, ?, ?, ?, ?);";
    static const char *delete_sign_query =
        "delete from sign where x = ? and y = ? and z = ? and face = ?;";
    static const char *delete_signs_query =
        "delete from sign where x = ? and y = ? and z = ?;";
    static const char *load_blocks_query =
        "select x, y, z, w from block where p = ? and q = ?;";
    static const char *load_extras_query =
        "select x, y, z, w from extra where p = ? and q = ?;";
    static const char *load_lights_query =
        "select x, y, z, w from light where p = ? and q = ?;";
    static const char *load_shapes_query =
        "select x, y, z, w from shape where p = ? and q = ?;";
    static const char *load_transforms_query =
        "select x, y, z, w from transform where p = ? and q = ?;";
    static const char *load_signs_query =
        "select x, y, z, face, text from sign where p = ? and q = ?;";
    static const char *get_sign_query =
        "select text from sign where p = ? and q = ? and x = ? and y = ? and z = ? and face = ?;";
    static const char *get_light_query =
        "select w from light where p = ? and q = ? and x = ? and y = ? and z = ?";
    static const char *get_key_query =
        "select key from key where p = ? and q = ?;";
    static const char *set_key_query =
        "insert or replace into key (p, q, key) "
        "values (?, ?, ?);";
    static const char *get_option_query =
        "select value from option where name = ?;";
    static const char *set_option_query =
        "insert or replace into option (name, value) "
        "values (?, ?);";
    int rc;
    rc = sqlite3_open(path, &db);
    if (rc) return rc;
    rc = sqlite3_exec(db, create_query, NULL, NULL, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(
        db, insert_block_query, -1, &insert_block_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(
        db, insert_extra_query, -1, &insert_extra_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(
        db, insert_light_query, -1, &insert_light_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(
        db, insert_shape_query, -1, &insert_shape_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(
        db, insert_transform_query, -1, &insert_transform_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(
        db, insert_sign_query, -1, &insert_sign_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(
        db, delete_sign_query, -1, &delete_sign_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(
        db, delete_signs_query, -1, &delete_signs_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(db, load_blocks_query, -1, &load_blocks_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(db, load_extras_query, -1, &load_extras_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(db, load_lights_query, -1, &load_lights_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(db, load_shapes_query, -1, &load_shapes_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(db, load_transforms_query, -1, &load_transforms_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(db, load_signs_query, -1, &load_signs_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(db, get_sign_query, -1, &get_sign_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(db, get_light_query, -1, &get_light_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(db, get_key_query, -1, &get_key_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(db, set_key_query, -1, &set_key_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(db, get_option_query, -1, &get_option_stmt, NULL);
    if (rc) return rc;
    rc = sqlite3_prepare_v2(db, set_option_query, -1, &set_option_stmt, NULL);
    if (rc) return rc;
    sqlite3_exec(db, "begin;", NULL, NULL, NULL);
    db_worker_start();
    return 0;
}

void db_close(void) {
    if (!db_enabled) {
        return;
    }
    db_worker_stop();
    sqlite3_exec(db, "commit;", NULL, NULL, NULL);
    sqlite3_finalize(insert_block_stmt);
    sqlite3_finalize(insert_extra_stmt);
    sqlite3_finalize(insert_light_stmt);
    sqlite3_finalize(insert_shape_stmt);
    sqlite3_finalize(insert_transform_stmt);
    sqlite3_finalize(insert_sign_stmt);
    sqlite3_finalize(delete_sign_stmt);
    sqlite3_finalize(delete_signs_stmt);
    sqlite3_finalize(load_blocks_stmt);
    sqlite3_finalize(load_extras_stmt);
    sqlite3_finalize(load_lights_stmt);
    sqlite3_finalize(load_shapes_stmt);
    sqlite3_finalize(load_transforms_stmt);
    sqlite3_finalize(load_signs_stmt);
    sqlite3_finalize(get_sign_stmt);
    sqlite3_finalize(get_light_stmt);
    sqlite3_finalize(get_key_stmt);
    sqlite3_finalize(set_key_stmt);
    sqlite3_finalize(get_option_stmt);
    sqlite3_finalize(set_option_stmt);
    sqlite3_close(db);
}

void db_commit(void) {
    if (!db_enabled) {
        return;
    }
    mtx_lock(&mtx);
    ring_put_commit(&ring);
    cnd_signal(&cnd);
    mtx_unlock(&mtx);
}

void _db_commit(void) {
    sqlite3_exec(db, "commit; begin;", NULL, NULL, NULL);
}

void db_clear_state(void) {
    if (!db_enabled) {
        return;
    }
    sqlite3_exec(db, "delete from state;", NULL, NULL, NULL);
}

void db_save_state(float x, float y, float z, float rx, float ry) {
    if (!db_enabled) {
        return;
    }
    static const char *query =
        "insert into state (x, y, z, rx, ry) values (?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_double(stmt, 1, x);
    sqlite3_bind_double(stmt, 2, y);
    sqlite3_bind_double(stmt, 3, z);
    sqlite3_bind_double(stmt, 4, rx);
    sqlite3_bind_double(stmt, 5, ry);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

int db_load_state(float *x, float *y, float *z, float *rx, float *ry,
                  int player) {
    if (!db_enabled) {
        return 0;
    }
    static const char *query =
        "select x, y, z, rx, ry from state;";
    int result = 0;
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    for (int i=0; i<=player; i++) {
        result = sqlite3_step(stmt);
    }
    if (result == SQLITE_ROW) {
        *x = sqlite3_column_double(stmt, 0);
        *y = sqlite3_column_double(stmt, 1);
        *z = sqlite3_column_double(stmt, 2);
        *rx = sqlite3_column_double(stmt, 3);
        *ry = sqlite3_column_double(stmt, 4);
        result = 1;
    } else {
        result = 0;
    }
    sqlite3_finalize(stmt);
    return result;
}

void db_clear_player_names(void) {
    if (!db_enabled) {
        return;
    }
    sqlite3_exec(db, "delete from player_name;", NULL, NULL, NULL);
}

void db_save_player_name(const char *name) {
    if (!db_enabled) {
        return;
    }
    static const char *query = "insert into player_name (name) values (?);";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, name, -1, NULL);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

int db_load_player_name(char *name, int max_name_length, int player) {
    if (!db_enabled) {
        return 0;
    }
    static const char *query = "select name from player_name;";
    int result = 0;
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    for (int i=0; i<=player; i++) {
        result = sqlite3_step(stmt);
    }
    if (result == SQLITE_ROW) {
        strncpy(name, (char *)sqlite3_column_text(stmt, 0), max_name_length);
        name[max_name_length-1] = '\0';
        result = 1;
    } else {
        result = 0;
    }
    sqlite3_finalize(stmt);
    return result;
}

void db_insert_block(int p, int q, int x, int y, int z, int w) {
    if (!db_enabled) {
        return;
    }
    mtx_lock(&mtx);
    ring_put_block(&ring, p, q, x, y, z, w);
    cnd_signal(&cnd);
    mtx_unlock(&mtx);
}

void _db_insert_block(int p, int q, int x, int y, int z, int w) {
    sqlite3_reset(insert_block_stmt);
    sqlite3_bind_int(insert_block_stmt, 1, p);
    sqlite3_bind_int(insert_block_stmt, 2, q);
    sqlite3_bind_int(insert_block_stmt, 3, x);
    sqlite3_bind_int(insert_block_stmt, 4, y);
    sqlite3_bind_int(insert_block_stmt, 5, z);
    sqlite3_bind_int(insert_block_stmt, 6, w);
    sqlite3_step(insert_block_stmt);
}

void db_insert_extra(int p, int q, int x, int y, int z, int w) {
    if (!db_enabled) {
        return;
    }
    mtx_lock(&mtx);
    ring_put_extra(&ring, p, q, x, y, z, w);
    cnd_signal(&cnd);
    mtx_unlock(&mtx);
}

void _db_insert_extra(int p, int q, int x, int y, int z, int w) {
    sqlite3_reset(insert_extra_stmt);
    sqlite3_bind_int(insert_extra_stmt, 1, p);
    sqlite3_bind_int(insert_extra_stmt, 2, q);
    sqlite3_bind_int(insert_extra_stmt, 3, x);
    sqlite3_bind_int(insert_extra_stmt, 4, y);
    sqlite3_bind_int(insert_extra_stmt, 5, z);
    sqlite3_bind_int(insert_extra_stmt, 6, w);
    sqlite3_step(insert_extra_stmt);
}

void db_insert_light(int p, int q, int x, int y, int z, int w) {
    if (!db_enabled) {
        return;
    }
    mtx_lock(&mtx);
    ring_put_light(&ring, p, q, x, y, z, w);
    cnd_signal(&cnd);
    mtx_unlock(&mtx);
}

void _db_insert_light(int p, int q, int x, int y, int z, int w) {
    sqlite3_reset(insert_light_stmt);
    sqlite3_bind_int(insert_light_stmt, 1, p);
    sqlite3_bind_int(insert_light_stmt, 2, q);
    sqlite3_bind_int(insert_light_stmt, 3, x);
    sqlite3_bind_int(insert_light_stmt, 4, y);
    sqlite3_bind_int(insert_light_stmt, 5, z);
    sqlite3_bind_int(insert_light_stmt, 6, w);
    sqlite3_step(insert_light_stmt);
}

int db_get_light(int p, int q, int x, int y, int z) {
    if (!db_enabled) {
        return 0;
    }
    sqlite3_reset(get_light_stmt);
    sqlite3_bind_int(get_light_stmt, 1, p);
    sqlite3_bind_int(get_light_stmt, 2, q);
    sqlite3_bind_int(get_light_stmt, 3, x);
    sqlite3_bind_int(get_light_stmt, 4, y);
    sqlite3_bind_int(get_light_stmt, 5, z);
    if (sqlite3_step(get_light_stmt) == SQLITE_ROW) {
        return sqlite3_column_int(get_light_stmt, 0);
    }
    return 0;
}

void db_insert_shape(int p, int q, int x, int y, int z, int w) {
    if (!db_enabled) {
        return;
    }
    mtx_lock(&mtx);
    ring_put_shape(&ring, p, q, x, y, z, w);
    cnd_signal(&cnd);
    mtx_unlock(&mtx);
}

void _db_insert_shape(int p, int q, int x, int y, int z, int w) {
    sqlite3_reset(insert_shape_stmt);
    sqlite3_bind_int(insert_shape_stmt, 1, p);
    sqlite3_bind_int(insert_shape_stmt, 2, q);
    sqlite3_bind_int(insert_shape_stmt, 3, x);
    sqlite3_bind_int(insert_shape_stmt, 4, y);
    sqlite3_bind_int(insert_shape_stmt, 5, z);
    sqlite3_bind_int(insert_shape_stmt, 6, w);
    sqlite3_step(insert_shape_stmt);
}

void db_insert_transform(int p, int q, int x, int y, int z, int w) {
    if (!db_enabled) {
        return;
    }
    mtx_lock(&mtx);
    ring_put_transform(&ring, p, q, x, y, z, w);
    cnd_signal(&cnd);
    mtx_unlock(&mtx);
}

void _db_insert_transform(int p, int q, int x, int y, int z, int w) {
    sqlite3_reset(insert_transform_stmt);
    sqlite3_bind_int(insert_transform_stmt, 1, p);
    sqlite3_bind_int(insert_transform_stmt, 2, q);
    sqlite3_bind_int(insert_transform_stmt, 3, x);
    sqlite3_bind_int(insert_transform_stmt, 4, y);
    sqlite3_bind_int(insert_transform_stmt, 5, z);
    sqlite3_bind_int(insert_transform_stmt, 6, w);
    sqlite3_step(insert_transform_stmt);
}

void db_insert_sign(
    int p, int q, int x, int y, int z, int face, const char *text)
{
    if (!db_enabled) {
        return;
    }
    sqlite3_reset(insert_sign_stmt);
    sqlite3_bind_int(insert_sign_stmt, 1, p);
    sqlite3_bind_int(insert_sign_stmt, 2, q);
    sqlite3_bind_int(insert_sign_stmt, 3, x);
    sqlite3_bind_int(insert_sign_stmt, 4, y);
    sqlite3_bind_int(insert_sign_stmt, 5, z);
    sqlite3_bind_int(insert_sign_stmt, 6, face);
    sqlite3_bind_text(insert_sign_stmt, 7, text, -1, NULL);
    sqlite3_step(insert_sign_stmt);
}

void db_delete_sign(int x, int y, int z, int face) {
    if (!db_enabled) {
        return;
    }
    sqlite3_reset(delete_sign_stmt);
    sqlite3_bind_int(delete_sign_stmt, 1, x);
    sqlite3_bind_int(delete_sign_stmt, 2, y);
    sqlite3_bind_int(delete_sign_stmt, 3, z);
    sqlite3_bind_int(delete_sign_stmt, 4, face);
    sqlite3_step(delete_sign_stmt);
}

void db_delete_signs(int x, int y, int z) {
    if (!db_enabled) {
        return;
    }
    sqlite3_reset(delete_signs_stmt);
    sqlite3_bind_int(delete_signs_stmt, 1, x);
    sqlite3_bind_int(delete_signs_stmt, 2, y);
    sqlite3_bind_int(delete_signs_stmt, 3, z);
    sqlite3_step(delete_signs_stmt);
}

void db_delete_all_signs(void) {
    if (!db_enabled) {
        return;
    }
    sqlite3_exec(db, "delete from sign;", NULL, NULL, NULL);
}

void db_load_blocks(Map *map, int p, int q) {
    if (!db_enabled) {
        return;
    }
    mtx_lock(&load_mtx);
    sqlite3_reset(load_blocks_stmt);
    sqlite3_bind_int(load_blocks_stmt, 1, p);
    sqlite3_bind_int(load_blocks_stmt, 2, q);
    while (sqlite3_step(load_blocks_stmt) == SQLITE_ROW) {
        int x = sqlite3_column_int(load_blocks_stmt, 0);
        int y = sqlite3_column_int(load_blocks_stmt, 1);
        int z = sqlite3_column_int(load_blocks_stmt, 2);
        int w = sqlite3_column_int(load_blocks_stmt, 3);
        map_set(map, x, y, z, w);
    }
    mtx_unlock(&load_mtx);
}

void db_load_extras(Map *map, int p, int q) {
    if (!db_enabled) {
        return;
    }
    mtx_lock(&load_mtx);
    sqlite3_reset(load_extras_stmt);
    sqlite3_bind_int(load_extras_stmt, 1, p);
    sqlite3_bind_int(load_extras_stmt, 2, q);
    while (sqlite3_step(load_extras_stmt) == SQLITE_ROW) {
        int x = sqlite3_column_int(load_extras_stmt, 0);
        int y = sqlite3_column_int(load_extras_stmt, 1);
        int z = sqlite3_column_int(load_extras_stmt, 2);
        int w = sqlite3_column_int(load_extras_stmt, 3);
        map_set(map, x, y, z, w);
    }
    mtx_unlock(&load_mtx);
}

void db_load_lights(Map *map, int p, int q) {
    if (!db_enabled) {
        return;
    }
    mtx_lock(&load_mtx);
    sqlite3_reset(load_lights_stmt);
    sqlite3_bind_int(load_lights_stmt, 1, p);
    sqlite3_bind_int(load_lights_stmt, 2, q);
    while (sqlite3_step(load_lights_stmt) == SQLITE_ROW) {
        int x = sqlite3_column_int(load_lights_stmt, 0);
        int y = sqlite3_column_int(load_lights_stmt, 1);
        int z = sqlite3_column_int(load_lights_stmt, 2);
        int w = sqlite3_column_int(load_lights_stmt, 3);
        map_set(map, x, y, z, w);
    }
    mtx_unlock(&load_mtx);
}

void db_load_shapes(Map *map, int p, int q) {
    if (!db_enabled) {
        return;
    }
    mtx_lock(&load_mtx);
    sqlite3_reset(load_shapes_stmt);
    sqlite3_bind_int(load_shapes_stmt, 1, p);
    sqlite3_bind_int(load_shapes_stmt, 2, q);
    while (sqlite3_step(load_shapes_stmt) == SQLITE_ROW) {
        int x = sqlite3_column_int(load_shapes_stmt, 0);
        int y = sqlite3_column_int(load_shapes_stmt, 1);
        int z = sqlite3_column_int(load_shapes_stmt, 2);
        int w = sqlite3_column_int(load_shapes_stmt, 3);
        map_set(map, x, y, z, w);
    }
    mtx_unlock(&load_mtx);
}

void db_load_transforms(Map *map, int p, int q) {
    if (!db_enabled) {
        return;
    }
    mtx_lock(&load_mtx);
    sqlite3_reset(load_transforms_stmt);
    sqlite3_bind_int(load_transforms_stmt, 1, p);
    sqlite3_bind_int(load_transforms_stmt, 2, q);
    while (sqlite3_step(load_transforms_stmt) == SQLITE_ROW) {
        int x = sqlite3_column_int(load_transforms_stmt, 0);
        int y = sqlite3_column_int(load_transforms_stmt, 1);
        int z = sqlite3_column_int(load_transforms_stmt, 2);
        int w = sqlite3_column_int(load_transforms_stmt, 3);
        map_set(map, x, y, z, w);
    }
    mtx_unlock(&load_mtx);
}

void db_load_signs(SignList *list, int p, int q) {
    if (!db_enabled) {
        return;
    }
    mtx_lock(&load_mtx);
    sqlite3_reset(load_signs_stmt);
    sqlite3_bind_int(load_signs_stmt, 1, p);
    sqlite3_bind_int(load_signs_stmt, 2, q);
    while (sqlite3_step(load_signs_stmt) == SQLITE_ROW) {
        int x = sqlite3_column_int(load_signs_stmt, 0);
        int y = sqlite3_column_int(load_signs_stmt, 1);
        int z = sqlite3_column_int(load_signs_stmt, 2);
        int face = sqlite3_column_int(load_signs_stmt, 3);
        const char *text = (const char *)sqlite3_column_text(
            load_signs_stmt, 4);
        sign_list_add(list, x, y, z, face, text);
    }
    mtx_unlock(&load_mtx);
}

const unsigned char *db_get_sign(int p, int q, int x, int y, int z, int face) {
    if (!db_enabled) {
        return NULL;
    }
    sqlite3_reset(get_sign_stmt);
    sqlite3_bind_int(get_sign_stmt, 1, p);
    sqlite3_bind_int(get_sign_stmt, 2, q);
    sqlite3_bind_int(get_sign_stmt, 3, x);
    sqlite3_bind_int(get_sign_stmt, 4, y);
    sqlite3_bind_int(get_sign_stmt, 5, z);
    sqlite3_bind_int(get_sign_stmt, 6, face);
    if (sqlite3_step(get_sign_stmt) == SQLITE_ROW) {
        return sqlite3_column_text(get_sign_stmt, 0);
    }
    return NULL;
}

int db_get_key(int p, int q) {
    if (!db_enabled) {
        return 0;
    }
    sqlite3_reset(get_key_stmt);
    sqlite3_bind_int(get_key_stmt, 1, p);
    sqlite3_bind_int(get_key_stmt, 2, q);
    if (sqlite3_step(get_key_stmt) == SQLITE_ROW) {
        return sqlite3_column_int(get_key_stmt, 0);
    }
    return 0;
}

void db_set_key(int p, int q, int key) {
    if (!db_enabled) {
        return;
    }
    mtx_lock(&mtx);
    ring_put_key(&ring, p, q, key);
    cnd_signal(&cnd);
    mtx_unlock(&mtx);
}

void _db_set_key(int p, int q, int key) {
    sqlite3_reset(set_key_stmt);
    sqlite3_bind_int(set_key_stmt, 1, p);
    sqlite3_bind_int(set_key_stmt, 2, q);
    sqlite3_bind_int(set_key_stmt, 3, key);
    sqlite3_step(set_key_stmt);
}

void db_set_option(char *name, char *value) {
    if (!db_enabled) {
        return;
    }
    sqlite3_reset(set_option_stmt);
    sqlite3_bind_text(set_option_stmt, 1, name, -1, NULL);
    sqlite3_bind_text(set_option_stmt, 2, value, -1, NULL);
    sqlite3_step(set_option_stmt);
}

const unsigned char *db_get_option(char *name) {
    if (!db_enabled) {
        return NULL;
    }
    sqlite3_reset(get_option_stmt);
    sqlite3_bind_text(get_option_stmt, 1, name, -1, NULL);
    if (sqlite3_step(get_option_stmt) == SQLITE_ROW) {
        return sqlite3_column_text(get_option_stmt, 0);
    }
    return NULL;
}

void db_worker_start(void) {
    if (!db_enabled) {
        return;
    }
    ring_alloc(&ring, 1024);
    mtx_init(&mtx, mtx_plain);
    mtx_init(&load_mtx, mtx_plain);
    cnd_init(&cnd);
    thrd_create(&thrd, db_worker_run, NULL);
}

void db_worker_stop(void) {
    if (!db_enabled) {
        return;
    }
    mtx_lock(&mtx);
    ring_put_exit(&ring);
    cnd_signal(&cnd);
    mtx_unlock(&mtx);
    thrd_join(thrd, NULL);
    cnd_destroy(&cnd);
    mtx_destroy(&load_mtx);
    mtx_destroy(&mtx);
    ring_free(&ring);
}

int db_worker_run(__attribute__((unused)) void *arg) {
    int running = 1;
    while (running) {
        RingEntry e;
        mtx_lock(&mtx);
        while (!ring_get(&ring, &e)) {
            cnd_wait(&cnd, &mtx);
        }
        mtx_unlock(&mtx);
        switch (e.type) {
            case BLOCK:
                _db_insert_block(e.p, e.q, e.x, e.y, e.z, e.w);
                break;
            case EXTRA:
                _db_insert_extra(e.p, e.q, e.x, e.y, e.z, e.w);
                break;
            case SHAPE:
                _db_insert_shape(e.p, e.q, e.x, e.y, e.z, e.w);
                break;
            case TRANSFORM:
                _db_insert_transform(e.p, e.q, e.x, e.y, e.z, e.w);
                break;
            case LIGHT:
                _db_insert_light(e.p, e.q, e.x, e.y, e.z, e.w);
                break;
            case SIGN:
                // Signs are directly saved to game DB when setting them, so
                // this branch is currently never used.
                // If you change this to support SIGN in Ring here then
                // remember to call free(e.sign) here as well.
                break;
            case KEY:
                _db_set_key(e.p, e.q, e.key);
                break;
            case COMMIT:
                _db_commit();
                break;
            case EXIT:
                running = 0;
                break;
        }
    }
    return 0;
}
