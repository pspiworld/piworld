#pragma once

#define LUA_MAXINPUT 1024

void pwlua_parse_line(int player_id, const char *buffer);
void pwlua_control_callback(int player_id, int x, int y, int z, int face);
void set_control_block_callback(int player_id, const char *text);

