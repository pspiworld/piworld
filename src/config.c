
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "x11_event_handler.h"
#include "config.h"

Config _config;
Config *config = &_config;

void reset_config() {
    get_config_path(config->path);
    get_default_db_path(config->db_path);
    config->port = DEFAULT_PORT;
    config->server[0] = '\0';
    strncpy(config->window_title, WINDOW_TITLE, strlen(WINDOW_TITLE));
    config->verbose = 0;
    config->window_x = CENTRE_IN_SCREEN;
    config->window_y = CENTRE_IN_SCREEN;
    config->window_width = WINDOW_WIDTH;
    config->window_height = WINDOW_HEIGHT;
}

void get_config_path(char *path)
{
#ifdef RELEASE
    snprintf(path, MAX_PATH_LENGTH, "%s/.piworld", getenv("HOME"));
    mkdir(path, 0755);
#else
    // Keep the config and world database in the current dir for dev builds
    snprintf(path, MAX_PATH_LENGTH, ".");
#endif
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

    while (1) {
        int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            {"port",         required_argument, 0,  0 },
            {"server",       required_argument, 0,  0 },
            {"verbose",      no_argument,       0,  0 },
            {"window-size",  required_argument, 0,  0 },
            {"window-title", required_argument, 0,  0 },
            {"window-xy",    required_argument, 0,  0 },
            {0,              0,                 0,  0 }
        };

        c = getopt_long(argc, argv, "", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 0:
            opt_name = long_options[option_index].name;
            if (opt_name == "port") {
                sscanf(optarg, "%d", &config->port);
            } else if (opt_name == "verbose") {
                config->verbose = 1;
            } else if (opt_name == "window-size") {
                sscanf(optarg, "%d,%d", &config->window_width,
                       &config->window_height);
            } else if (opt_name == "window-title") {
                if (strlen(optarg) < MAX_TITLE_LENGTH) {
                    strncpy(config->window_title, optarg, MAX_TITLE_LENGTH);
                }
            } else if (opt_name == "window-xy") {
                sscanf(optarg, "%d,%d", &config->window_x, &config->window_y);
            } else if (opt_name == "server") {
                if (strlen(optarg) < MAX_ADDR_LENGTH) {
                    strncpy(config->server, optarg, MAX_ADDR_LENGTH);
                }
            }
            break;

        case '?':
            break;

        default:
            printf("?? getopt returned character code 0%o ??\n", c);
        }
    }

    if (optind < argc) {
        strncpy(config->db_path, argv[optind++], MAX_PATH_LENGTH);
        printf("unused ARGV-elements: ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        printf("\n");
    }
}

