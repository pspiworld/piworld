#pragma once

#include <GLES2/gl2.h>
#include <stdlib.h>
#include "client.h"
#include "chunk.h"
#include "local_player.h"
#include "player.h"
#include "pwlua.h"
#include "sign.h"
#include "tinycthread.h"
#include "ui.h"
#include "util.h"

#define UNASSIGNED -1

void pw_init(void);
void pw_deinit(void);
void pw_connect_to_server(char *server_addr, int server_port);
char *get_db_path(void);
void pw_setup_window(void);
int check_mode_changed(void);
int check_time_changed(void);
int check_render_option_changed(void);
void pw_unload_game(void);
int get_delete_radius(void);
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
void set_time_elapsed_and_day_length(float elapsed, int day_length);
void map_set_func(int x, int y, int z, int w, void *arg);
void drain_edit_queue(size_t max_items, double max_time, double now);
void toggle_observe_view(LocalPlayer *p);
void toggle_picture_in_picture_observe_view(LocalPlayer *p);
void cycle_item_in_hand_down(LocalPlayer *player);
void cycle_item_in_hand_up(LocalPlayer *player);
void ensure_chunks(Player *player);
void pw_exit(void);
void pw_new_game(char *path);
void pw_load_game(char *path);
void set_worldgen(char *worldgen);
int player_intersects_block(
    int height,
    float x, float y, float z,
    int hx, int hy, int hz);
size_t get_float_size(void);
void gen_sign_chunk_buffer(Chunk *chunk);
void get_sight_vector(float rx, float ry, float *vx, float *vy, float *vz);
void set_view_radius(int requested_size, int delete_request);
void set_render_radius(int radius);
void set_delete_radius(int radius);
void recheck_view_radius(void);
void recheck_players_view_size(void);
void change_player_count(int player_count);
void remove_player(LocalPlayer *local);
LocalPlayer *add_player_on_new_device(void);
int is_online(void);
int is_offline(void);
void set_show_clouds(int option);
void set_show_lights(int option);
void set_show_plants(int option);
void set_show_trees(int option);
GLuint gen_player_buffer(float x, float y, float z, float rx, float ry, int p);
void update_player(Player *player,
    float x, float y, float z, float rx, float ry, int interpolate);
void interpolate_player(Player *player);
void set_player_count(Client *client, int count);
float get_scale_factor(void);
void render_player_world(LocalPlayer *local, FPS fps);
void login(void);
void reset_model(void);
void initialize_worker_threads(void);
void deinitialize_worker_threads(void);

