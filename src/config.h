#pragma once

// app parameters
#define WINDOW_TITLE "PiWorld (Esc to exit)"
#define FULLSCREEN 0
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 400
#define VSYNC 1
#define MAX_MESSAGES 4
#define DB_FILENAME "my.piworld"
#define USE_CACHE 0
#define DAY_LENGTH 600
#define INVERT_MOUSE 0

// rendering options
#define SHOW_LIGHTS 1
#define SHOW_PLANTS 1
#define SHOW_CLOUDS 1
#define SHOW_TREES 1
#define SHOW_ITEM 1
#define SHOW_CROSSHAIRS 1
#define SHOW_WIREFRAME 1
#define SHOW_INFO_TEXT 1
#define SHOW_CHAT_TEXT 1
#define SHOW_PLAYER_NAMES 1

// key bindings
#define CRAFT_KEY_CHAT 't'
#define CRAFT_KEY_COMMAND '/'
#define CRAFT_KEY_SIGN '`'

// advanced parameters
#define CHUNK_SIZE 16
#define COMMIT_INTERVAL 5
#define DEFAULT_PORT 4080
#define MAX_ADDR_LENGTH 196
#define MAX_PATH_LENGTH 512
#define MAX_DIR_LENGTH 256
#define MAX_FILENAME_LENGTH 196
#define MAX_TITLE_LENGTH 256
#define AUTO_PICK_RADIUS -1
#ifdef MESA
#define HFLOAT_CONFIG 0
#else
#define HFLOAT_CONFIG 1
#endif

typedef struct {
    char path[MAX_DIR_LENGTH];
    char db_path[MAX_PATH_LENGTH];
    int fullscreen;
    int players;
    int port;
    int show_chat_text;
    int show_clouds;
    int show_crosshairs;
    int show_info_text;
    int show_item;
    int show_lights;
    int show_plants;
    int show_player_names;
    int show_trees;
    int show_wireframe;
    char server[MAX_ADDR_LENGTH];
    int use_cache;
    int verbose;
    int view;
    int vsync;
    char window_title[MAX_TITLE_LENGTH];
    int window_x;
    int window_y;
    int window_width;
    int window_height;
    int benchmark_create_chunks;
    int no_limiters;
    int delete_radius;
    int time;
    int use_hfloat;
} Config;

extern Config *config;

void reset_config(void);
void get_config_path(char *path);
void get_default_db_path(char *path);
void get_server_db_cache_path(char *path);
void parse_startup_config(int argc, char **argv);

