/*
Lua startup script handling, including registering of any on_* event handlers.
*/

#include <stdio.h>
#include <unistd.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "local_player.h"
#include "local_players.h"
#include "pw.h"
#include "pwlua.h"
#include "pwlua_api.h"
#include "stb_ds.h"
#include "ui.h"

lua_State* startup_lua_instance;

void pwlua_startup_init(void)
{
    char path[MAX_PATH_LENGTH];
    lua_State* L = NULL;

    startup_lua_instance = NULL;
    snprintf(path, MAX_PATH_LENGTH, "%s/startup.lua", config->path);
    if (access(path, F_OK) == -1) {
        return;  // startup.lua not found
    }

    startup_lua_instance = luaL_newstate();
    L = startup_lua_instance;

    luaL_openlibs(L);

    pwlua_api_add_functions(L);
    pwlua_api_add_constants(L);

    lua_getglobal(L, "dofile");
    lua_pushstring(L, path);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        printf("error running function 'dofile(\"startup.lua\")': %s\n",
               lua_tostring(L, -1));
    }
}

void pwlua_on_player_init(LocalPlayer *local)
{
    lua_State* L = startup_lua_instance;
    if (L == NULL) {
        return;
    }

    lua_pushnumber(L, local->player->id);
    lua_setglobal(L, "player_id");
    lua_getglobal(L, "on_player_init");
    if (lua_type(L, 1) != LUA_TFUNCTION) {
        lua_pop(L, 1);
        return;  // no function named on_player_init in startup.lua
    }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        printf("error running function 'on_player_init': %s\n",
               lua_tostring(L, -1));
    }
}

int pwlua_on_menu_event(LocalPlayer *local, Menu *menu, int event)
{
    lua_State* L = startup_lua_instance;
    if (L == NULL) {
        return 0;
    }
    lua_pushnumber(L, local->player->id);
    lua_setglobal(L, "player_id");
    lua_getglobal(L, "on_menu_event");
    if (lua_type(L, 1) != LUA_TFUNCTION) {
        lua_pop(L, 1);
        return 0;  // no function named on_menu_event in startup.lua
    }

    // Event table
    lua_newtable(L);

    lua_pushinteger(L, menu->id);
    lua_setfield(L, -2, "menu_id");

    lua_pushinteger(L, event);
    lua_setfield(L, -2, "menu_item_id");

    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        printf("error running function 'on_menu_event': %s\n",
               lua_tostring(L, -1));
        return 0;
    }
    /* retrieve result */
    int result = lua_toboolean(L, -1);
    lua_pop(L, 1); /* pop returned value */
    return result;
}

void pwlua_startup_deinit(void)
{
    if (startup_lua_instance != NULL) {
        lua_close(startup_lua_instance);
    }
}

void pwlua_startup_call_action(LocalPlayer *local, char *action_name, Event *event)
{
    lua_State* L = startup_lua_instance;
    if (L == NULL) {
        return;
    }
    lua_pushnumber(L, local->player->id);
    lua_setglobal(L, "player_id");
    lua_getglobal(L, action_name);

    // Event table
    lua_newtable(L);

    lua_pushinteger(L, event->type);
    lua_setfield(L, -2, "type");

    lua_pushinteger(L, event->button);
    lua_setfield(L, -2, "button");

    lua_pushinteger(L, event->state);
    lua_setfield(L, -2, "state");

    lua_pushnumber(L, event->x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, event->y);
    lua_setfield(L, -2, "y");

    lua_pushinteger(L, event->mods);
    lua_setfield(L, -2, "mods");

    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        printf("error running function '%s': %s\n",
               lua_tostring(L, -1), action_name);
    }
}

