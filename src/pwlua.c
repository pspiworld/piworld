
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdio.h>
#include "config.h"
#include "db.h"
#include "item.h"
#include "pw.h"
#include "tinycthread.h"

#define AVAILABLE 0
#define STARTING 1
#define STOPPED 2

#define LUA_MAXINPUT 512

typedef struct  {
    thrd_t thread;
    lua_State *L;
    char lua_code[LUA_MAXINPUT];
    int player_id;
    int state;
} LuaThreadState;

int pwlua_thread_run(LuaThreadState *lts);

static int pwlua_echo(lua_State *L);
static int pwlua_get_block(lua_State *L);
static int pwlua_set_block(lua_State *L);
static int pwlua_get_crosshair(lua_State *L);
static int pwlua_get_player_pos(lua_State *L);
static int pwlua_set_player_pos(lua_State *L);
static int pwlua_get_player_angle(lua_State *L);
static int pwlua_set_player_angle(lua_State *L);
static int pwlua_get_sign(lua_State *L);
static int pwlua_set_sign(lua_State *L);
static int pwlua_get_time(lua_State *L);
static int pwlua_set_time(lua_State *L);

// One Lua thread per local player.
LuaThreadState p1;
LuaThreadState p2;
LuaThreadState p3;
LuaThreadState p4;

void pwlua_parse_line(int player_id, const char *buffer)
{
    LuaThreadState *lts;
    if (player_id == 1) {
        lts = &p1;
    } else if (player_id == 2) {
        lts = &p2;
    } else if (player_id == 3) {
        lts = &p3;
    } else if (player_id == 4) {
        lts = &p4;
    } else {
        return;
    }
    if (lts->state == STOPPED) {
        thrd_join(lts->thread, NULL);
        lts->state = AVAILABLE;
    }

    if (lts->state == AVAILABLE) {
        lts->player_id = player_id;
        lts->state = STARTING;
        snprintf(lts->lua_code, LUA_MAXINPUT, "%s", buffer + 1);
        thrd_create(&lts->thread, (void *)pwlua_thread_run, lts);
    }
}

int pwlua_thread_run(LuaThreadState *lts)
{
    lua_State *L = luaL_newstate();
    lts->L = L;

    luaL_openlibs(L);

    lua_pushnumber(L, lts->player_id);
    lua_setglobal(L, "player_id");

    lua_register(L, "echo", pwlua_echo);
    lua_register(L, "get_block", pwlua_get_block);
    lua_register(L, "set_block", pwlua_set_block);
    lua_register(L, "get_crosshair", pwlua_get_crosshair);
    lua_register(L, "get_player_pos", pwlua_get_player_pos);
    lua_register(L, "set_player_pos", pwlua_set_player_pos);
    lua_register(L, "get_player_angle", pwlua_get_player_angle);
    lua_register(L, "set_player_angle", pwlua_set_player_angle);
    lua_register(L, "get_sign", pwlua_get_sign);
    lua_register(L, "set_sign", pwlua_set_sign);
    lua_register(L, "get_time", pwlua_get_time);
    lua_register(L, "set_time", pwlua_set_time);

#define PUSH_BLOCK_TYPE(b) { lua_pushnumber(L, b); \
    lua_setglobal(L, ""#b""); }

    PUSH_BLOCK_TYPE(EMPTY);
    PUSH_BLOCK_TYPE(GRASS);
    PUSH_BLOCK_TYPE(SAND);
    PUSH_BLOCK_TYPE(STONE);
    PUSH_BLOCK_TYPE(BRICK);
    PUSH_BLOCK_TYPE(WOOD);
    PUSH_BLOCK_TYPE(CEMENT);
    PUSH_BLOCK_TYPE(DIRT);
    PUSH_BLOCK_TYPE(PLANK);
    PUSH_BLOCK_TYPE(SNOW);
    PUSH_BLOCK_TYPE(GLASS);
    PUSH_BLOCK_TYPE(COBBLE);
    PUSH_BLOCK_TYPE(LIGHT_STONE);
    PUSH_BLOCK_TYPE(DARK_STONE);
    PUSH_BLOCK_TYPE(CHEST);
    PUSH_BLOCK_TYPE(LEAVES);
    PUSH_BLOCK_TYPE(TALL_GRASS);
    PUSH_BLOCK_TYPE(YELLOW_FLOWER);
    PUSH_BLOCK_TYPE(RED_FLOWER);
    PUSH_BLOCK_TYPE(PURPLE_FLOWER);
    PUSH_BLOCK_TYPE(SUN_FLOWER);
    PUSH_BLOCK_TYPE(WHITE_FLOWER);
    PUSH_BLOCK_TYPE(BLUE_FLOWER);
    PUSH_BLOCK_TYPE(COLOR_00);
    PUSH_BLOCK_TYPE(COLOR_01);
    PUSH_BLOCK_TYPE(COLOR_02);
    PUSH_BLOCK_TYPE(COLOR_03);
    PUSH_BLOCK_TYPE(COLOR_04);
    PUSH_BLOCK_TYPE(COLOR_05);
    PUSH_BLOCK_TYPE(COLOR_06);
    PUSH_BLOCK_TYPE(COLOR_07);
    PUSH_BLOCK_TYPE(COLOR_08);
    PUSH_BLOCK_TYPE(COLOR_09);
    PUSH_BLOCK_TYPE(COLOR_10);
    PUSH_BLOCK_TYPE(COLOR_11);
    PUSH_BLOCK_TYPE(COLOR_12);
    PUSH_BLOCK_TYPE(COLOR_13);
    PUSH_BLOCK_TYPE(COLOR_14);
    PUSH_BLOCK_TYPE(COLOR_15);
    PUSH_BLOCK_TYPE(COLOR_16);
    PUSH_BLOCK_TYPE(COLOR_17);
    PUSH_BLOCK_TYPE(COLOR_18);
    PUSH_BLOCK_TYPE(COLOR_19);
    PUSH_BLOCK_TYPE(COLOR_20);
    PUSH_BLOCK_TYPE(COLOR_21);
    PUSH_BLOCK_TYPE(COLOR_22);
    PUSH_BLOCK_TYPE(COLOR_23);
    PUSH_BLOCK_TYPE(COLOR_24);
    PUSH_BLOCK_TYPE(COLOR_25);
    PUSH_BLOCK_TYPE(COLOR_26);
    PUSH_BLOCK_TYPE(COLOR_27);
    PUSH_BLOCK_TYPE(COLOR_28);
    PUSH_BLOCK_TYPE(COLOR_29);
    PUSH_BLOCK_TYPE(COLOR_30);
    PUSH_BLOCK_TYPE(COLOR_31);

    if (luaL_dostring(L, lts->lua_code)) {
        lua_close(L);
        lts->state = STOPPED;
        return -1;
    }

    lua_close(L);
    lts->state = STOPPED;
    return 0;
}

static int pwlua_echo(lua_State *L)
{
    int player_id;
    int argcount = lua_gettop(L);
    const char *text;
    if (argcount == 1) {
        text = lua_tolstring(L, 1, NULL);
        lua_getglobal(L, "player_id");
        player_id = lua_tointeger(L, 2);
    } else if (argcount == 2) {
        player_id = lua_tointeger(L, 1);
        text = lua_tolstring(L, 2, NULL);
    } else {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
        return 0;
    }
    add_message(player_id, text);
    return 0;
}

static int pwlua_get_block(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 3) {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
    }
    int x, y, z, w;
    x = lua_tointeger(L, 1);
    y = lua_tointeger(L, 2);
    z = lua_tointeger(L, 3);
    w = get_block(x, y, z);
    lua_pushinteger(L, w);
    return 1;
}

static int pwlua_set_block(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 4) {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
    }
    int x, y, z, w;
    x = lua_tointeger(L, 1);
    y = lua_tointeger(L, 2);
    z = lua_tointeger(L, 3);
    w = lua_tointeger(L, 4);
    mtx_lock(&force_chunks_mtx);
    set_block(x, y, z, w);
    mtx_unlock(&force_chunks_mtx);
    return 0;
}

static int pwlua_get_crosshair(lua_State *L)
{
    int player_id;
    int argcount = lua_gettop(L);
    if (argcount == 0) {
        lua_getglobal(L, "player_id");
    } else if (argcount == 1) {
        // player id passed as the single argument
    } else {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
    }
    player_id = lua_tointeger(L, 1);
    int x, y, z, face;
    pw_get_crosshair(player_id, &x, &y, &z, &face);
    lua_pushinteger(L, x);
    lua_pushinteger(L, y);
    lua_pushinteger(L, z);
    lua_pushinteger(L, face);
    return 4;
}

static int pwlua_get_player_pos(lua_State *L)
{
    int player_id;
    int argcount = lua_gettop(L);
    if (argcount == 0) {
        lua_getglobal(L, "player_id");
    } else if (argcount == 1) {
        // player id passed as the single argument
    } else {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
    }
    player_id = lua_tointeger(L, 1);
    float x, y, z;
    pw_get_player_pos(player_id, &x, &y, &z);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, z);
    return 3;
}

static int pwlua_set_player_pos(lua_State *L)
{
    int player_id;
    float x, y, z;
    int argcount = lua_gettop(L);
    if (argcount == 3) {
        lua_getglobal(L, "player_id");
        x = lua_tonumber(L, 1);
        y = lua_tonumber(L, 2);
        z = lua_tonumber(L, 3);
        player_id = lua_tointeger(L, 4);
    } else if (argcount == 4) {
        player_id = lua_tointeger(L, 1);
        x = lua_tonumber(L, 2);
        y = lua_tonumber(L, 3);
        z = lua_tonumber(L, 4);
    } else {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
        return 0;
    }
    pw_set_player_pos(player_id, x, y, z);
    return 0;
}

static int pwlua_get_player_angle(lua_State *L)
{
    int player_id;
    int argcount = lua_gettop(L);
    if (argcount == 0) {
        lua_getglobal(L, "player_id");
    } else if (argcount == 1) {
        // player id passed as the single argument
    } else {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
    }
    player_id = lua_tointeger(L, 1);
    float x, y;
    pw_get_player_angle(player_id, &x, &y);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

static int pwlua_set_player_angle(lua_State *L)
{
    int player_id;
    float x, y;
    int argcount = lua_gettop(L);
    if (argcount == 2) {
        lua_getglobal(L, "player_id");
        x = lua_tonumber(L, 1);
        y = lua_tonumber(L, 2);
        player_id = lua_tointeger(L, 3);
    } else if (argcount == 3) {
        player_id = lua_tointeger(L, 1);
        x = lua_tonumber(L, 2);
        y = lua_tonumber(L, 3);
    } else {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
        return 0;
    }
    pw_set_player_angle(player_id, x, y);
    return 0;
}

static int pwlua_get_sign(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 4) {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
    }
    int x, y, z, face;
    x = lua_tointeger(L, 1);
    y = lua_tointeger(L, 2);
    z = lua_tointeger(L, 3);
    face = lua_tointeger(L, 4);
    const char *existing_sign = (const char *) db_get_sign(
        chunked(x), chunked(z), x, y, z, face);
    lua_pushstring(L, existing_sign);
    return 1;
}

static int pwlua_set_sign(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 5) {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
    }
    if (!lua_isstring(L, 5)) {
        lua_pushstring(L, "incorrect argument type");
        lua_error(L);
    }
    int x, y, z, face;
    const char *text;
    x = lua_tointeger(L, 1);
    y = lua_tointeger(L, 2);
    z = lua_tointeger(L, 3);
    face = lua_tointeger(L, 4);
    text = lua_tolstring(L, 5, NULL);
    mtx_lock(&force_chunks_mtx);
    set_sign(x, y, z, face, text);
    mtx_unlock(&force_chunks_mtx);
    return 0;
}

static int pwlua_get_time(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 0) {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
    }
    int time = pw_get_time();
    lua_pushinteger(L, time);
    return 1;
}

static int pwlua_set_time(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 1) {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
    }
    if (!lua_isnumber(L, 1)) {
        lua_pushstring(L, "incorrect argument type");
        lua_error(L);
    }
    int time = lua_tointeger(L, 1);
    pw_set_time(time);
    return 0;
}

