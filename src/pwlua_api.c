
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "config.h"
#include "db.h"
#include "item.h"
#include "noise.h"
#include "pw.h"
#include "pwlua.h"
#include "world.h"

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
static int pwlua_get_light(lua_State *L);
static int pwlua_set_light(lua_State *L);
static int pwlua_get_control(lua_State *L);
static int pwlua_set_control(lua_State *L);
static int pwlua_set_control_callback(lua_State *L);
static int pwlua_get_shape(lua_State *L);
static int pwlua_set_shape(lua_State *L);
static int pwlua_get_transform(lua_State *L);
static int pwlua_set_transform(lua_State *L);
static int pwlua_get_open(lua_State *L);
static int pwlua_set_open(lua_State *L);
static int pwlua_set_shell(lua_State *L);
static int pwlua_sync_world(lua_State *L);

static int pwlua_map_set(lua_State *L);
static int pwlua_map_set_extra(lua_State *L);
static int pwlua_map_set_light(lua_State *L);
static int pwlua_map_set_shape(lua_State *L);
static int pwlua_map_set_transform(lua_State *L);
static int pwlua_map_set_sign(lua_State *L);
static int pwlua_simplex2(lua_State *L);
static int pwlua_simplex3(lua_State *L);

void pwlua_api_add_functions(lua_State *L)
{
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
    lua_register(L, "get_light", pwlua_get_light);
    lua_register(L, "set_light", pwlua_set_light);
    lua_register(L, "get_control", pwlua_get_control);
    lua_register(L, "set_control", pwlua_set_control);
    lua_register(L, "set_control_callback", pwlua_set_control_callback);
    lua_register(L, "get_shape", pwlua_get_shape);
    lua_register(L, "set_shape", pwlua_set_shape);
    lua_register(L, "get_transform", pwlua_get_transform);
    lua_register(L, "set_transform", pwlua_set_transform);
    lua_register(L, "get_open", pwlua_get_open);
    lua_register(L, "set_open", pwlua_set_open);
    lua_register(L, "set_shell", pwlua_set_shell);
    lua_register(L, "sync_world", pwlua_sync_world);
}

void pwlua_api_add_worldgen_functions(lua_State *L)
{
    lua_register(L, "map_set", pwlua_map_set);
    lua_register(L, "map_set_extra", pwlua_map_set_extra);
    lua_register(L, "map_set_light", pwlua_map_set_light);
    lua_register(L, "map_set_shape", pwlua_map_set_shape);
    lua_register(L, "map_set_transform", pwlua_map_set_transform);
    lua_register(L, "map_set_sign", pwlua_map_set_sign);
    lua_register(L, "simplex2", pwlua_simplex2);
    lua_register(L, "simplex3", pwlua_simplex3);
}

void pwlua_api_add_constants(lua_State *L)
{
#define PUSH_CONST(b) { lua_pushnumber(L, b); \
    lua_setglobal(L, ""#b""); }

    PUSH_CONST(CHUNK_SIZE);
    PUSH_CONST(BEDROCK);

    PUSH_CONST(EMPTY);
    PUSH_CONST(GRASS);
    PUSH_CONST(SAND);
    PUSH_CONST(STONE);
    PUSH_CONST(BRICK);
    PUSH_CONST(WOOD);
    PUSH_CONST(CEMENT);
    PUSH_CONST(DIRT);
    PUSH_CONST(PLANK);
    PUSH_CONST(SNOW);
    PUSH_CONST(GLASS);
    PUSH_CONST(COBBLE);
    PUSH_CONST(LIGHT_STONE);
    PUSH_CONST(DARK_STONE);
    PUSH_CONST(CHEST);
    PUSH_CONST(LEAVES);
    PUSH_CONST(TALL_GRASS);
    PUSH_CONST(YELLOW_FLOWER);
    PUSH_CONST(RED_FLOWER);
    PUSH_CONST(PURPLE_FLOWER);
    PUSH_CONST(SUN_FLOWER);
    PUSH_CONST(WHITE_FLOWER);
    PUSH_CONST(BLUE_FLOWER);
    PUSH_CONST(COLOR_00);
    PUSH_CONST(COLOR_01);
    PUSH_CONST(COLOR_02);
    PUSH_CONST(COLOR_03);
    PUSH_CONST(COLOR_04);
    PUSH_CONST(COLOR_05);
    PUSH_CONST(COLOR_06);
    PUSH_CONST(COLOR_07);
    PUSH_CONST(COLOR_08);
    PUSH_CONST(COLOR_09);
    PUSH_CONST(COLOR_10);
    PUSH_CONST(COLOR_11);
    PUSH_CONST(COLOR_12);
    PUSH_CONST(COLOR_13);
    PUSH_CONST(COLOR_14);
    PUSH_CONST(COLOR_15);
    PUSH_CONST(COLOR_16);
    PUSH_CONST(COLOR_17);
    PUSH_CONST(COLOR_18);
    PUSH_CONST(COLOR_19);
    PUSH_CONST(COLOR_20);
    PUSH_CONST(COLOR_21);
    PUSH_CONST(COLOR_22);
    PUSH_CONST(COLOR_23);
    PUSH_CONST(COLOR_24);
    PUSH_CONST(COLOR_25);
    PUSH_CONST(COLOR_26);
    PUSH_CONST(COLOR_27);
    PUSH_CONST(COLOR_28);
    PUSH_CONST(COLOR_29);
    PUSH_CONST(COLOR_30);
    PUSH_CONST(COLOR_31);

    PUSH_CONST(CUBE);
    PUSH_CONST(SLAB1);
    PUSH_CONST(SLAB2);
    PUSH_CONST(SLAB3);
    PUSH_CONST(SLAB4);
    PUSH_CONST(SLAB5);
    PUSH_CONST(SLAB6);
    PUSH_CONST(SLAB7);
    PUSH_CONST(SLAB8);
    PUSH_CONST(SLAB9);
    PUSH_CONST(SLAB10);
    PUSH_CONST(SLAB11);
    PUSH_CONST(SLAB12);
    PUSH_CONST(SLAB13);
    PUSH_CONST(SLAB14);
    PUSH_CONST(SLAB15);
    PUSH_CONST(UPPER_DOOR);
    PUSH_CONST(LOWER_DOOR);

    PUSH_CONST(DOOR_X);
    PUSH_CONST(DOOR_X_PLUS);
    PUSH_CONST(DOOR_Z);
    PUSH_CONST(DOOR_Z_PLUS);
    PUSH_CONST(DOOR_X_FLIP);
    PUSH_CONST(DOOR_X_PLUS_FLIP);
    PUSH_CONST(DOOR_Z_FLIP);
    PUSH_CONST(DOOR_Z_PLUS_FLIP);
}

#define ERROR_ARG_COUNT (luaL_error(L, "incorrect argument count"))

static int pwlua_echo(lua_State *L)
{
    int player_id;
    int argcount = lua_gettop(L);
    const char *text;
    if (argcount == 1) {
        text = lua_tolstring(L, 1, NULL);
        lua_getglobal(L, "player_id");
        player_id = luaL_checkint(L, 2);
    } else if (argcount == 2) {
        player_id = luaL_checkint(L, 1);
        text = lua_tolstring(L, 2, NULL);
    } else {
        return ERROR_ARG_COUNT;
    }
    add_message(player_id, text);
    return 0;
}

static int pwlua_get_block(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 3) {
        return ERROR_ARG_COUNT;
    }
    int x, y, z, w;
    x = luaL_checkint(L, 1);
    y = luaL_checkint(L, 2);
    z = luaL_checkint(L, 3);
    w = get_block(x, y, z);
    lua_pushinteger(L, w);
    return 1;
}

static int pwlua_set_block(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 4) {
        return ERROR_ARG_COUNT;
    }
    int x, y, z, w;
    x = luaL_checkint(L, 1);
    y = luaL_checkint(L, 2);
    z = luaL_checkint(L, 3);
    w = luaL_checkint(L, 4);
    queue_set_block(x, y, z, w);
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
        return ERROR_ARG_COUNT;
    }
    player_id = luaL_checkint(L, 1);
    int x, y, z, face, result;
    result = pw_get_crosshair(player_id, &x, &y, &z, &face);
    if (!result) {
        // Crosshair has not highlighted a block
        return 0;
    }
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
        return ERROR_ARG_COUNT;
    }
    player_id = luaL_checkint(L, 1);
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
        player_id = luaL_checkint(L, 4);
    } else if (argcount == 4) {
        player_id = luaL_checkint(L, 1);
        x = lua_tonumber(L, 2);
        y = lua_tonumber(L, 3);
        z = lua_tonumber(L, 4);
    } else {
        return ERROR_ARG_COUNT;
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
        return ERROR_ARG_COUNT;
    }
    player_id = luaL_checkint(L, 1);
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
        player_id = luaL_checkint(L, 3);
    } else if (argcount == 3) {
        player_id = luaL_checkint(L, 1);
        x = lua_tonumber(L, 2);
        y = lua_tonumber(L, 3);
    } else {
        return ERROR_ARG_COUNT;
    }
    pw_set_player_angle(player_id, x, y);
    return 0;
}

static int pwlua_get_sign(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 4) {
        return ERROR_ARG_COUNT;
    }
    int x, y, z, face;
    x = luaL_checkint(L, 1);
    y = luaL_checkint(L, 2);
    z = luaL_checkint(L, 3);
    face = luaL_checkint(L, 4);
    const char *existing_sign = (const char *) get_sign(
        chunked(x), chunked(z), x, y, z, face);
    lua_pushstring(L, existing_sign);
    return 1;
}

static int pwlua_set_sign(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 5) {
        return ERROR_ARG_COUNT;
    }
    if (!lua_isstring(L, 5)) {
        return luaL_error(L, "incorrect argument type");
    }
    int x, y, z, face;
    const char *text;
    x = luaL_checkint(L, 1);
    y = luaL_checkint(L, 2);
    z = luaL_checkint(L, 3);
    face = luaL_checkint(L, 4);
    text = lua_tolstring(L, 5, NULL);
    queue_set_sign(x, y, z, face, text);
    return 0;
}

static int pwlua_get_time(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 0) {
        return ERROR_ARG_COUNT;
    }
    int time = pw_get_time();
    lua_pushinteger(L, time);
    return 1;
}

static int pwlua_set_time(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 1) {
        return ERROR_ARG_COUNT;
    }
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "incorrect argument type");
    }
    int time = luaL_checkint(L, 1);
    pw_set_time(time);
    return 0;
}

static int pwlua_get_light(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 3) {
        return ERROR_ARG_COUNT;
    }
    int x, y, z, w;
    x = luaL_checkint(L, 1);
    y = luaL_checkint(L, 2);
    z = luaL_checkint(L, 3);
    w = get_light(chunked(x), chunked(z), x, y, z);
    lua_pushinteger(L, w);
    return 1;
}

static int pwlua_set_light(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 4) {
        return ERROR_ARG_COUNT;
    }
    int x, y, z, w;
    x = luaL_checkint(L, 1);
    y = luaL_checkint(L, 2);
    z = luaL_checkint(L, 3);
    w = luaL_checkint(L, 4);
    queue_set_light(x, y, z, w);
    return 0;
}

static int pwlua_get_control(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 3) {
        return ERROR_ARG_COUNT;
    }
    int x, y, z, w;
    x = luaL_checkint(L, 1);
    y = luaL_checkint(L, 2);
    z = luaL_checkint(L, 3);
    w = is_control(get_extra(x, y, z));
    lua_pushinteger(L, w);
    return 1;
}

static int pwlua_set_control(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 4) {
        return ERROR_ARG_COUNT;
    }
    int x, y, z, w;
    x = luaL_checkint(L, 1);
    y = luaL_checkint(L, 2);
    z = luaL_checkint(L, 3);
    w = luaL_checkint(L, 4);
    if (w) {
        w = get_extra(x, y, z) | EXTRA_BIT_CONTROL;
    } else {
        w = get_extra(x, y, z) & ~EXTRA_BIT_CONTROL;
    }
    queue_set_extra(x, y, z, w);
    return 0;
}

static int pwlua_set_control_callback(lua_State *L)
{
    int argcount = lua_gettop(L);
    const char *text;
    if (argcount != 1) {
        return ERROR_ARG_COUNT;
    }
    text = lua_tolstring(L, 1, NULL);
    set_control_block_callback(L, text);
    return 0;
}

static int pwlua_get_shape(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 3) {
        return ERROR_ARG_COUNT;
    }
    int x, y, z, w;
    x = luaL_checkint(L, 1);
    y = luaL_checkint(L, 2);
    z = luaL_checkint(L, 3);
    w = get_shape(x, y, z);
    lua_pushinteger(L, w);
    return 1;
}

static int pwlua_set_shape(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 4) {
        return ERROR_ARG_COUNT;
    }
    int x, y, z, w;
    x = luaL_checkint(L, 1);
    y = luaL_checkint(L, 2);
    z = luaL_checkint(L, 3);
    w = luaL_checkint(L, 4);
    queue_set_shape(x, y, z, w);
    return 0;
}

static int pwlua_get_transform(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 3) {
        return ERROR_ARG_COUNT;
    }
    int x, y, z, w;
    x = luaL_checkint(L, 1);
    y = luaL_checkint(L, 2);
    z = luaL_checkint(L, 3);
    w = get_transform(x, y, z);
    lua_pushinteger(L, w);
    return 1;
}

static int pwlua_set_transform(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 4) {
        return ERROR_ARG_COUNT;
    }
    int x, y, z, w;
    x = luaL_checkint(L, 1);
    y = luaL_checkint(L, 2);
    z = luaL_checkint(L, 3);
    w = luaL_checkint(L, 4);
    queue_set_transform(x, y, z, w);
    return 0;
}

static int pwlua_get_open(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 3) {
        return ERROR_ARG_COUNT;
    }
    int x, y, z, w;
    x = luaL_checkint(L, 1);
    y = luaL_checkint(L, 2);
    z = luaL_checkint(L, 3);
    w = is_open(get_extra(x, y, z));
    lua_pushinteger(L, w);
    return 1;
}

static int pwlua_set_open(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 4) {
        return ERROR_ARG_COUNT;
    }
    int x, y, z, w;
    x = luaL_checkint(L, 1);
    y = luaL_checkint(L, 2);
    z = luaL_checkint(L, 3);
    w = luaL_checkint(L, 4);
    if (w) {
        w = get_extra(x, y, z) | EXTRA_BIT_OPEN;
    } else {
        w = get_extra(x, y, z) & ~EXTRA_BIT_OPEN;
    }
    queue_set_extra(x, y, z, w);
    return 0;
}

static int pwlua_map_set(lua_State *L)
{
    int n = lua_gettop(L);    /* number of arguments */
    if (n != 4) {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
    }
    int x, y, z, w;
    void *block_map;

    lua_getglobal(L, "_block_map");
    block_map = lua_touserdata(L, -1);

    x = lua_tointeger(L, 1);
    y = lua_tointeger(L, 2);
    z = lua_tointeger(L, 3);
    w = lua_tointeger(L, 4);
    map_set_func(x, y, z, w, block_map);
    return 0;
}

static int pwlua_map_set_extra(lua_State *L)
{
    int n = lua_gettop(L);    /* number of arguments */
    if (n != 4) {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
    }
    int x, y, z, w;
    void *extra_map;

    lua_getglobal(L, "_extra_map");
    extra_map = lua_touserdata(L, -1);

    x = lua_tointeger(L, 1);
    y = lua_tointeger(L, 2);
    z = lua_tointeger(L, 3);
    w = lua_tointeger(L, 4);
    map_set_func(x, y, z, w, extra_map);
    return 0;
}

static int pwlua_map_set_light(lua_State *L)
{
    int n = lua_gettop(L);    /* number of arguments */
    if (n != 4) {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
    }
    int x, y, z, w;
    void *light_map;

    lua_getglobal(L, "_light_map");
    light_map = lua_touserdata(L, -1);

    x = lua_tointeger(L, 1);
    y = lua_tointeger(L, 2);
    z = lua_tointeger(L, 3);
    w = lua_tointeger(L, 4);
    map_set_func(x, y, z, w, light_map);
    return 0;
}

static int pwlua_map_set_shape(lua_State *L)
{
    int n = lua_gettop(L);    /* number of arguments */
    if (n != 4) {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
    }
    int x, y, z, w;
    void *shape_map;

    lua_getglobal(L, "_shape_map");
    shape_map = lua_touserdata(L, -1);

    x = lua_tointeger(L, 1);
    y = lua_tointeger(L, 2);
    z = lua_tointeger(L, 3);
    w = lua_tointeger(L, 4);
    map_set_func(x, y, z, w, shape_map);
    return 0;
}

static int pwlua_map_set_transform(lua_State *L)
{
    int n = lua_gettop(L);    /* number of arguments */
    if (n != 4) {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
    }
    int x, y, z, w;
    void *transform_map;

    lua_getglobal(L, "_transform_map");
    transform_map = lua_touserdata(L, -1);

    x = lua_tointeger(L, 1);
    y = lua_tointeger(L, 2);
    z = lua_tointeger(L, 3);
    w = lua_tointeger(L, 4);
    map_set_func(x, y, z, w, transform_map);
    return 0;
}

static int pwlua_map_set_sign(lua_State *L)
{
    int n = lua_gettop(L);    /* number of arguments */
    if (n != 5) {
        lua_pushstring(L, "incorrect argument count");
        lua_error(L);
    }
    int x, y, z, face;
    const char *text;
    void *sign_list;

    lua_getglobal(L, "_sign_list");
    sign_list = lua_touserdata(L, -1);

    x = lua_tointeger(L, 1);
    y = lua_tointeger(L, 2);
    z = lua_tointeger(L, 3);
    face = lua_tointeger(L, 4);
    text = lua_tolstring(L, 5, NULL);
    worldgen_set_sign(x, y, z, face, text, sign_list);
    return 0;
}

static int pwlua_set_shell(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 1) {
        return ERROR_ARG_COUNT;
    }
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "incorrect argument type");
    }
    int shell = luaL_checkint(L, 1);
    pwlua_set_is_shell(L, shell);
    return 0;
}

static int pwlua_sync_world(lua_State *L)
{
    int argcount = lua_gettop(L);
    if (argcount != 0) {
        return ERROR_ARG_COUNT;
    }
    drain_edit_queue(100000, 1, 0);
    return 0;
}

static int pwlua_simplex2(lua_State *L)
{
    float x, y;
    int octaves;
    float persistence;
    float lacunarity;
    x = lua_tonumber(L, 1);
    y = lua_tonumber(L, 2);
    octaves = lua_tointeger(L, 3);
    persistence = lua_tonumber(L, 4);
    lacunarity = lua_tonumber(L, 5);
    float v = simplex2(x, y, octaves, persistence, lacunarity);
    lua_pushnumber(L, v);
    return 1;
}

static int pwlua_simplex3(lua_State *L)
{
    float x, y, z;
    int octaves;
    float persistence;
    float lacunarity;
    x = lua_tonumber(L, 1);
    y = lua_tonumber(L, 2);
    z = lua_tonumber(L, 3);
    octaves = lua_tointeger(L, 4);
    persistence = lua_tonumber(L, 5);
    lacunarity = lua_tonumber(L, 6);
    float v = simplex3(x, y, z, octaves, persistence, lacunarity);
    lua_pushnumber(L, v);
    return 1;
}

