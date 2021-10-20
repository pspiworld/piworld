#pragma once

#include "pw.h"

#define MAX_CLIENTS 128

extern Client clients[MAX_CLIENTS];
extern int client_count;

void clients_reset(void);
Client *find_client(int id);
void delete_client(int id);
void delete_all_players(void);
int get_first_active_player(Client *client);
void parse_buffer(char *buffer);

