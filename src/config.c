
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "x11_event_handler.h"
#include "config.h"

Config _config;
Config *config = &_config;

void reset_config() {
    strncpy(config->db_path, DB_PATH, strlen(DB_PATH));
    config->port = DEFAULT_PORT;
    config->server[0] = '\0';
    strncpy(config->window_title, WINDOW_TITLE, strlen(WINDOW_TITLE));
    config->verbose = 0;
    config->window_x = CENTRE_IN_SCREEN;
    config->window_y = CENTRE_IN_SCREEN;
    config->window_width = WINDOW_WIDTH;
    config->window_height = WINDOW_HEIGHT;
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

