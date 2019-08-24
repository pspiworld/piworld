#pragma once

void client_enable();
void client_disable();
int get_client_enabled();
void client_connect(char *hostname, int port);
void client_start();
void client_stop();
void client_send(char *data);
char *client_recv();
void client_version(int version);
void client_login(const char *username, const char *identity_token);
void client_nick(const int player, const char *name);
void client_spawn(const int player);
void client_goto(const int player, const char *name);
void client_pq(int player, int p, int q);
void client_position(int player, float x, float y, float z, float rx, float ry);
void client_add_player(int player);
void client_remove_player(int player);
void client_chunk(int p, int q, int key);
void client_block(int x, int y, int z, int w);
void client_light(int x, int y, int z, int w);
void client_sign(int x, int y, int z, int face, const char *text);
void client_talk(const char *text);

