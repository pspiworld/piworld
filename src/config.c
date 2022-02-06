
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "x11_event_handler.h"
#include "config.h"
#include "pg.h"
#include "util.h"

Config _config;
Config *config = &_config;

void reset_config() {
    get_config_path(config->path);
    get_default_db_path(config->db_path);
    config->fullscreen = FULLSCREEN;
#if RASPI > 0
    config->fullscreen_width = 720;
#else
    config->fullscreen_width = 0;
#endif
    config->fullscreen_height = 0;
    config->lua_standalone = 0;
    config->players = -1;
    config->port = DEFAULT_PORT;
    config->server[0] = '\0';
    config->show_chat_text = SHOW_CHAT_TEXT;
    config->show_clouds = SHOW_CLOUDS;
    config->show_crosshairs = SHOW_CROSSHAIRS;
    config->show_info_text = SHOW_INFO_TEXT;
    config->show_item = SHOW_ITEM;
    config->show_lights = SHOW_LIGHTS;
    config->show_plants = SHOW_PLANTS;
    config->show_player_names = SHOW_PLAYER_NAMES;
    config->show_trees = SHOW_TREES;
    config->show_wireframe = SHOW_WIREFRAME;
    config->use_cache = USE_CACHE;
    config->verbose = 0;
    config->view = AUTO_PICK_RADIUS;
    config->vsync = VSYNC;
    strncpy(config->window_title, WINDOW_TITLE, sizeof(config->window_title));
    config->window_title[sizeof(WINDOW_TITLE)] = '\0';
    config->window_x = CENTRE_IN_SCREEN;
    config->window_y = CENTRE_IN_SCREEN;
    config->window_width = WINDOW_WIDTH;
    config->window_height = WINDOW_HEIGHT;
    config->benchmark_create_chunks = 0;
    config->no_limiters = 0;
    config->delete_radius = AUTO_PICK_RADIUS;
    config->time = -1;
    config->use_hfloat = HFLOAT_CONFIG;
    strncpy(config->worldgen_path, WORLDGEN_PATH, sizeof(config->worldgen_path));
    config->worldgen_path[sizeof(WORLDGEN_PATH)] = '\0';
    config->ignore_gamepad = 0;
    config->always_use_osk = 0;
    config->bindings[0] = '\0';
}

void get_config_path(char *path)
{
    if (RELEASE_BUILD || access(get_data_dir(), W_OK) != 0) {
        snprintf(path, MAX_DIR_LENGTH, "%s/.piworld", getenv("HOME"));
        mkdir(path, 0755);
    } else {
        // Keep the config and world database in the current dir for dev builds
        snprintf(path, MAX_DIR_LENGTH, get_data_dir());
    }
}

void get_default_db_path(char *path)
{
    snprintf(path, MAX_PATH_LENGTH, "%s/%s", config->path, DB_FILENAME);
}

void get_server_db_cache_path(char *path)
{
    snprintf(path, MAX_PATH_LENGTH,
        "%s/cache.%s.%d.db", config->path, config->server, config->port);
}

void parse_startup_config(int argc, char **argv) {
    int c;
    const char *opt_name;
    char separator[2];

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"fullscreen",        no_argument,       0,  0 },
            {"fullscreen-size",   required_argument, 0,  0 },
            {"lua-standalone",    no_argument,       0,  0 },
            {"players",           required_argument, 0,  0 },
            {"port",              required_argument, 0,  0 },
            {"server",            required_argument, 0,  0 },
            {"show-chat-text",    required_argument, 0,  0 },
            {"show-crosshairs",   required_argument, 0,  0 },
            {"show-clouds",       required_argument, 0,  0 },
            {"show-info-text",    required_argument, 0,  0 },
            {"show-item",         required_argument, 0,  0 },
            {"show-lights",       required_argument, 0,  0 },
            {"show-plants",       required_argument, 0,  0 },
            {"show-player-names", required_argument, 0,  0 },
            {"show-trees",        required_argument, 0,  0 },
            {"show-wireframe",    required_argument, 0,  0 },
            {"verbose",           no_argument,       0,  0 },
            {"use-cache",         required_argument, 0,  0 },
            {"view",              required_argument, 0,  0 },
            {"vsync",             required_argument, 0,  0 },
            {"window-size",       required_argument, 0,  0 },
            {"window-title",      required_argument, 0,  0 },
            {"window-xy",         required_argument, 0,  0 },
            {"benchmark-create-chunks", required_argument, 0,  0 },
            {"no-limiters",       no_argument,       0,  0 },
            {"delete-radius",     required_argument, 0,  0 },
            {"time",              required_argument, 0,  0 },
            {"hfloat",            required_argument, 0,  0 },
            {"worldgen",          required_argument, 0,  0 },
            {"ignore-gamepad",    no_argument,       0,  0 },
            {"always-use-osk",    no_argument,       0,  0 },
            {"bind",              required_argument, 0,  0 },
            {0,                   0,                 0,  0 }
        };

        c = getopt_long(argc, argv, "", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 0:
            opt_name = long_options[option_index].name;
            if (strncmp(opt_name, "fullscreen-size", 15) == 0 &&
                       sscanf(optarg, "%d%1[,x]%d", &config->fullscreen_width,
                              separator,
                              &config->fullscreen_height) == 3) {
            } else if (strncmp(opt_name, "fullscreen", 10) == 0) {
                 config->fullscreen = 1;
            } else if (strncmp(opt_name, "lua-standalone", 14) == 0) {
                 config->lua_standalone = 1;
            } else if (strncmp(opt_name, "players", 7) == 0 &&
                sscanf(optarg, "%d", &config->players) == 1) {
            } else if (strncmp(opt_name, "port", 4) == 0 &&
                sscanf(optarg, "%d", &config->port) == 1) {
            } else if (strncmp(opt_name, "show-chat-text", 15) == 0 &&
                       sscanf(optarg, "%d", &config->show_chat_text) == 1) {
            } else if (strncmp(opt_name, "show-crosshairs", 15) == 0 &&
                       sscanf(optarg, "%d", &config->show_crosshairs) == 1) {
            } else if (strncmp(opt_name, "show-clouds", 11) == 0 &&
                       sscanf(optarg, "%d", &config->show_clouds) == 1) {
            } else if (strncmp(opt_name, "show-info-text", 14) == 0 &&
                       sscanf(optarg, "%d", &config->show_info_text) == 1) {
            } else if (strncmp(opt_name, "show-item", 9) == 0 &&
                       sscanf(optarg, "%d", &config->show_item) == 1) {
            } else if (strncmp(opt_name, "show-lights", 11) == 0 &&
                       sscanf(optarg, "%d", &config->show_lights) == 1) {
            } else if (strncmp(opt_name, "show-plants", 11) == 0 &&
                       sscanf(optarg, "%d", &config->show_plants) == 1) {
            } else if (strncmp(opt_name, "show-player-names", 17) == 0 &&
                       sscanf(optarg, "%d", &config->show_player_names) == 1) {
            } else if (strncmp(opt_name, "show-trees", 10) == 0 &&
                       sscanf(optarg, "%d", &config->show_trees) == 1) {
            } else if (strncmp(opt_name, "show-wireframe", 14) == 0 &&
                       sscanf(optarg, "%d", &config->show_wireframe) == 1) {
            } else if (strncmp(opt_name, "use-cache", 9) == 0 &&
                       sscanf(optarg, "%d", &config->use_cache) == 1) {
            } else if (strncmp(opt_name, "verbose", 7) == 0) {
                config->verbose = 1;
            } else if (strncmp(opt_name, "view", 4) == 0 &&
                       sscanf(optarg, "%d", &config->view) == 1) {
            } else if (strncmp(opt_name, "vsync", 5) == 0 &&
                       sscanf(optarg, "%d", &config->vsync) == 1) {
            } else if (strncmp(opt_name, "window-size", 11) == 0 &&
                       sscanf(optarg, "%d%1[,x]%d", &config->window_width,
                              separator,
                              &config->window_height) == 3) {
            } else if (strncmp(opt_name, "window-title", 12) == 0 &&
                       sscanf(optarg, "%256c", config->window_title) == 1) {
                config->window_title[MIN(strlen(optarg),
                                         MAX_TITLE_LENGTH - 1)] = '\0';
            } else if (strncmp(opt_name, "window-xy", 9) == 0 &&
                       sscanf(optarg, "%d%1[,x]%d", &config->window_x,
                              separator,
                              &config->window_y) == 3) {
            } else if (strncmp(opt_name, "server", 6) == 0 &&
                       sscanf(optarg, "%256c", config->server) == 1) {
                config->server[MIN(strlen(optarg),
                                   MAX_ADDR_LENGTH - 1)] = '\0';
            } else if (strncmp(opt_name, "benchmark-create-chunks", 23) == 0 &&
                       sscanf(optarg, "%d",
                              &config->benchmark_create_chunks) == 1) {
            } else if (strncmp(opt_name, "no-limiters", 11) == 0) {
                config->no_limiters = 1;
            } else if (strncmp(opt_name, "delete-radius", 13) == 0 &&
                       sscanf(optarg, "%d", &config->delete_radius) == 1) {
            } else if (strncmp(opt_name, "time", 4) == 0 &&
                       sscanf(optarg, "%d", &config->time) == 1) {
            } else if (strncmp(opt_name, "hfloat", 6) == 0 &&
                       sscanf(optarg, "%d", &config->use_hfloat) == 1) {
            } else if (strncmp(opt_name, "worldgen", 8) == 0 &&
                       sscanf(optarg, "%256c", config->worldgen_path) == 1) {
                config->worldgen_path[MIN(strlen(optarg),
                                          MAX_PATH_LENGTH - 1)] = '\0';
            } else if (strncmp(opt_name, "ignore-gamepad", 14) == 0) {
                config->ignore_gamepad = 1;
            } else if (strncmp(opt_name, "always-use-osk", 14) == 0) {
                config->always_use_osk = 1;
            } else if (strncmp(opt_name, "bind", 4) == 0 &&
                       sscanf(optarg, "%256c", config->bindings) == 1) {
                config->bindings[MIN(strlen(optarg),
                                 MAX_PATH_LENGTH - 1)] = '\0';
            } else {
                printf("Bad argument for: --%s: %s\n", opt_name, optarg);
                exit(1);
            }
            break;

        case '?':
            // Exit on an unrecognised option
            exit(1);
            break;

        default:
            printf("?? getopt returned character code 0%o ??\n", c);
            exit(1);
        }
    }

    if (optind < argc) {
        strncpy(config->db_path, argv[optind++], MAX_DIR_LENGTH);
        if (optind < argc) {
            // Exit on extra unnamed arguments
            printf("unused ARGV-elements: ");
            while (optind < argc)
                printf("%s ", argv[optind++]);
            printf("\n");
            exit(1);
        }
    }
}

