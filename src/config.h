#pragma once

// app parameters
#define WINDOW_TITLE "PiWorld (Esc to exit)"
#define FULLSCREEN 0
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 400
#define VSYNC 1
#define MAX_MESSAGES 4
#define DB_FILENAME "my.piworld"
#define USE_CACHE 1
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
#define CRAFT_KEY_FORWARD 'w'
#define CRAFT_KEY_BACKWARD 's'
#define CRAFT_KEY_LEFT 'a'
#define CRAFT_KEY_RIGHT 'd'
#define CRAFT_KEY_JUMP GLFW_KEY_SPACE
#define CRAFT_KEY_FLY GLFW_KEY_TAB
#define CRAFT_KEY_OBSERVE 'o'
#define CRAFT_KEY_OBSERVE_INSET 'p'
#define CRAFT_KEY_ITEM_NEXT 'e'
#define CRAFT_KEY_ITEM_PREV 'r'
#define CRAFT_KEY_ZOOM GLFW_KEY_LEFT_SHIFT
#define CRAFT_KEY_ORTHO 'f'
#define CRAFT_KEY_CHAT 't'
#define CRAFT_KEY_COMMAND '/'
#define CRAFT_KEY_SIGN '`'

// advanced parameters
#define CHUNK_SIZE 16
#define COMMIT_INTERVAL 5
#define DEFAULT_PORT 4080
#define MAX_ADDR_LENGTH 256
#define MAX_PATH_LENGTH 256
#define MAX_TITLE_LENGTH 256

typedef struct {
    char path[MAX_PATH_LENGTH];
    char db_path[MAX_PATH_LENGTH];
    int port;
    char server[MAX_ADDR_LENGTH];
    int verbose;
    char window_title[MAX_TITLE_LENGTH];
    int window_x;
    int window_y;
    int window_width;
    int window_height;
} Config;

extern Config *config;

void reset_config();
void get_config_path(char *path);
void get_default_db_path(char *path);
void get_server_db_cache_path(char *path);
void parse_startup_config(int argc, char **argv);

