#pragma once

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "world.h"

void pwlua_worldgen(lua_State *L, int p, int q, void *arg);
lua_State *pwlua_worldgen_init(char *filename);

