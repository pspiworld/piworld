
#include "pw.h"
#include "pwlua_api.h"
#include "pwlua_worldgen.h"

lua_State *lua_worldgen_for_main_thread;
char *lua_worldgen_path;

void pwlua_worldgen(lua_State *L, int p, int q, void *block_map,
    void *extra_map, void *light_map, void *shape_map, void *sign_list,
    void *transform_map)
{
    lua_pushlightuserdata(L, block_map);
    lua_setglobal(L, "_block_map");
    lua_pushlightuserdata(L, extra_map);
    lua_setglobal(L, "_extra_map");
    lua_pushlightuserdata(L, light_map);
    lua_setglobal(L, "_light_map");
    lua_pushlightuserdata(L, shape_map);
    lua_setglobal(L, "_shape_map");
    lua_pushlightuserdata(L, sign_list);
    lua_setglobal(L, "_sign_list");
    lua_pushlightuserdata(L, transform_map);
    lua_setglobal(L, "_transform_map");

    lua_getfield(L, LUA_GLOBALSINDEX, "worldgen");  /* function to be called */
    lua_pushinteger(L, p);                          /* 1st argument */
    lua_pushinteger(L, q);                          /* 2nd argument */
    lua_call(L, 2, 0);     /* call 'worldgen' with 2 arguments and 0 results */
}

void pwlua_worldgen_init(char *filename)
{
    lua_worldgen_path = filename;
    lua_worldgen_for_main_thread = pwlua_worldgen_new_generator();
}

void pwlua_worldgen_deinit(void)
{
    lua_close(lua_worldgen_for_main_thread);
    lua_worldgen_for_main_thread = NULL;
}

lua_State *pwlua_worldgen_new_generator(void)
{
    lua_State *L = luaL_newstate();

    luaL_openlibs(L);
    pwlua_api_add_constants(L);
    pwlua_api_add_worldgen_functions(L);

    luaL_dofile(L, lua_worldgen_path);

    return L;
}

lua_State *pwlua_worldgen_get_main_thread_instance(void)
{
    return lua_worldgen_for_main_thread;
}

