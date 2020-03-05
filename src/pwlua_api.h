#pragma once

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

void pwlua_api_add_functions(lua_State *L);
void pwlua_api_add_worldgen_functions(lua_State *L);
void pwlua_api_add_constants(lua_State *L);

