#pragma once

#include "map.h"
#include "sign.h"

void db_enable(void);
void db_disable(void);
int get_db_enabled(void);
int db_init(char *path);
void db_close(void);
void db_commit(void);
void db_clear_state(void);
void db_save_state(float x, float y, float z, float rx, float ry);
int db_load_state(float *x, float *y, float *z, float *rx, float *ry,
                  int player);
void db_clear_player_names(void);
void db_save_player_name(const char *name);
int db_load_player_name(char *name, int max_name_length, int player);
void db_insert_block(int p, int q, int x, int y, int z, int w);
void db_insert_extra(int p, int q, int x, int y, int z, int w);
void db_insert_light(int p, int q, int x, int y, int z, int w);
void db_insert_sign(
    int p, int q, int x, int y, int z, int face, const char *text);
void db_delete_sign(int x, int y, int z, int face);
void db_delete_signs(int x, int y, int z);
void db_delete_all_signs(void);
void db_load_blocks(Map *map, int p, int q);
void db_load_extras(Map *map, int p, int q);
void db_load_lights(Map *map, int p, int q);
void db_load_signs(SignList *list, int p, int q);
const unsigned char *db_get_sign(int p, int q, int x, int y, int z, int face);
int db_get_light(int p, int q, int x, int y, int z);
int db_get_key(int p, int q);
void db_set_key(int p, int q, int key);
void db_set_option(char *name, char *value);
const unsigned char *db_get_option(char *name);
void db_worker_start(void);
void db_worker_stop(void);
int db_worker_run(void *arg);

