
#include "pw.h"
#include "pwlua_api.h"
#include "pwlua_worldgen.h"

void pwlua_worldgen(lua_State *L, int p, int q, void *arg)
{
    lua_pushlightuserdata(L, arg);
    lua_setglobal(L, "_block_map");

    lua_getfield(L, LUA_GLOBALSINDEX, "worldgen");  /* function to be called */
    lua_pushinteger(L, p);                          /* 1st argument */
    lua_pushinteger(L, q);                          /* 2nd argument */
    lua_call(L, 2, 0);     /* call 'worldgen' with 2 arguments and 0 results */
}

lua_State *pwlua_worldgen_init(char *filename)
{
    lua_State *L = luaL_newstate();

    pwlua_api_add_constants(L);
    pwlua_api_add_worldgen_functions(L);

    luaL_dofile(L, filename);

    return L;
}

