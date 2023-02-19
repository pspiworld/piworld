#pragma once

#include <lua.h>

#define LUA_MAXINPUT 1024
#define ERROR_ARG_COUNT (luaL_error(L, "incorrect argument count"))

typedef struct LuaThreadState LuaThreadState;

LuaThreadState *pwlua_new(int player_id);
void pwlua_remove(LuaThreadState *lts);
LuaThreadState *pwlua_new_shell(int player_id);
void pwlua_parse_line(LuaThreadState *lts, const char *buffer);
void pwlua_remove_closed_threads(void);
void pwlua_set_is_shell(lua_State *L, int is_shell);
void pwlua_control_callback(int player_id, int x, int y, int z, int face);
void set_control_block_callback(lua_State *L, const char *text);

