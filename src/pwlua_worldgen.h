#pragma once

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "world.h"

void pwlua_worldgen(lua_State *L, int p, int q, void *block_map,
    void *extra_map, void *light_map, void *shape_map, void *sign_list,
    void *transform_map);
lua_State *pwlua_worldgen_init(char *filename);

