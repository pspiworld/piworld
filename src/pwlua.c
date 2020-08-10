/*
** Lua REPL adapted from Lua 5.3
** See Copyright Notice at the end of this file
*/

#include <ctype.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "pw.h"
#include "pwlua.h"
#include "pwlua_api.h"
#include "tinycthread.h"
#ifdef RASPI
#include "RPi_GPIO_Lua_module.h"
#endif

#define AVAILABLE 0
#define STARTING 1
#define STOPPED 2
#define WAITING 3

#define LUA_MAX_CALLBACK_NAME 256

/* mark in error messages for incomplete statements */
#define EOFMARK		"<eof>"
#define marklen		(sizeof(EOFMARK)/sizeof(char) - 1)

struct LuaThreadState {
    thrd_t thread;
    mtx_t mtx;
    cnd_t cnd;
    lua_State *L;
    char lua_code[LUA_MAXINPUT];
    char control_callback[LUA_MAX_CALLBACK_NAME];
    int player_id;
    int state;
    int terminate;
    int is_shell;
    struct LuaThreadState *next, *prev;
};

static LuaThreadState *lua_threads;

int pwlua_thread_run(LuaThreadState *lts);


LuaThreadState *pwlua_new(int player_id)
{
    LuaThreadState *lts = malloc(sizeof(LuaThreadState));
    lts->lua_code[0] = '\0';
    lts->control_callback[0] = '\0';
    lts->player_id = player_id;
    lts->state = AVAILABLE;
    lts->is_shell = 0;
    lts->terminate = 0;
    if (lua_threads == NULL) {
        // First lua thread
        lua_threads = lts;
        lts->next = NULL;
        lts->prev = NULL;
    } else {
        // Insert at end of list
        LuaThreadState *last_lts = lua_threads;
        while (last_lts->next != NULL) {
            last_lts = last_lts->next;
        }
        last_lts->next = lts;
        lts->prev = last_lts;
        lts->next = NULL;
    }
    return lts;
}

LuaThreadState *pwlua_new_shell(int player_id)
{
    LuaThreadState *lts = pwlua_new(player_id);
    lts->is_shell = 1;
    return lts;
}

void pwlua_remove(LuaThreadState *lts)
{
    if (lts->prev == NULL && lts->next == NULL) {
        // No more Lua threads left
        lua_threads = NULL;
    } else {
        if (lts->prev == NULL) {
            lua_threads = lts->next;
            lts->next->prev = NULL;
        } else {
            if (lts->next == NULL) {
                lts->prev->next = NULL;
            } else {
                lts->prev->next = lts->next;
                lts->next->prev = lts->prev;
            }
        }
    }
    free(lts);
}

LuaThreadState *get_lua_state(lua_State *L)
{
    LuaThreadState *lts_match = NULL;
    for (LuaThreadState *lts=lua_threads; lts; lts=lts->next) {
        if (L == lts->L) {
            lts_match = lts;
            break;
        }
    }
    return lts_match;
}

void pwlua_set_is_shell(lua_State *L, int is_shell)
{
    LuaThreadState *lts = get_lua_state(L);
    if (lts == NULL) {
        return;
    }
    lts->is_shell = is_shell;
}

void pwlua_control_callback(int player_id, int x, int y, int z, int face)
{
    for (LuaThreadState *lts=lua_threads; lts; lts=lts->next) {
        char lua_code[LUA_MAXINPUT];
        if (strlen(lts->control_callback) == 0) {
            continue;
        }
        snprintf(lua_code, LUA_MAXINPUT, "$%s(%d,%d,%d,%d,%d)",
                 lts->control_callback, player_id, x, y, z, face);
        pwlua_parse_line(lts, lua_code);
    }
}

void set_control_block_callback(lua_State *L, const char *text)
{
    LuaThreadState *lts = get_lua_state(L);
    if (lts == NULL) {
        return;
    }
    if (text == NULL) {
        lts->control_callback[0] = '\0';
    } else {
        // Validate the callback function name by only allowing alphanumeric
        // and underscore characters.
        for (size_t i=0; i<strlen(text); i++) {
            char c = text[i];
            if (!isalnum(c) && c != '_') {
                printf("Invalid function name: %s\n", text);
                return;
            }
        }
        snprintf(lts->control_callback, LUA_MAX_CALLBACK_NAME, "%s", text);
    }
}

void pwlua_parse_line(LuaThreadState *lts, const char *buffer)
{
    if (lts == NULL) {
        return;
    }
    if (lts->state == STOPPED) {
        thrd_join(lts->thread, NULL);
        cnd_destroy(&lts->cnd);
        mtx_destroy(&lts->mtx);
        lts->state = AVAILABLE;
    }

    if (lts->state == AVAILABLE) {
        // Start the Lua thread and run the line of Lua code.
        lts->state = STARTING;
        snprintf(lts->lua_code, LUA_MAXINPUT, "%s", buffer + 1);
        lts->control_callback[0] = '\0';
        mtx_init(&lts->mtx, mtx_plain);
        cnd_init(&lts->cnd);
        thrd_create(&lts->thread, (void *)pwlua_thread_run, lts);
    } else if (lts->state == WAITING) {
        // Post Lua code line to thread and signal for it to run.
        snprintf(lts->lua_code, LUA_MAXINPUT, "%s", buffer + 1);
        cnd_signal(&lts->cnd);
    } else {
        char *still_running_msg = "Lua still running";
        add_message(lts->player_id, still_running_msg);
    }
}

void pwlua_remove_closed_threads(void)
{
    LuaThreadState *lts = lua_threads;
    while (lts) {
        LuaThreadState *lts_to_check = lts;
        lts = lts->next;
        if (lts_to_check->state == STOPPED && !lts_to_check->is_shell) {
            thrd_join(lts_to_check->thread, NULL);
            cnd_destroy(&lts_to_check->cnd);
            mtx_destroy(&lts_to_check->mtx);
            pwlua_remove(lts_to_check);
        }
    }
}

/*
** Try to compile line on the stack as 'return <line>;'; on return, stack
** has either compiled chunk or original line (if compilation failed).
*/
static int addreturn(lua_State *L)
{
    const char *line = lua_tostring(L, -1);  /* original line */
    const char *retline = lua_pushfstring(L, "return %s;", line);
    int status = luaL_loadbuffer(L, retline, strlen(retline), "=stdin");
    if (status == LUA_OK) {
        lua_remove(L, -2);  /* remove modified line */
    } else {
        lua_pop(L, 2); /* pop result from 'luaL_loadbuffer' and modified line */
    }
    return status;
}

/*
** Put line from LuaThreadState into stack.
*/
static int pushline(LuaThreadState *lts)
{
    lua_State *L = lts->L;
    char *b = lts->lua_code;
    size_t l;
    l = strlen(b);
    if (b[0] == '=') {  /* for compatibility with 5.2, ... */
        lua_pushfstring(L, "return %s", b + 1);  /* change '=' to 'return' */
    } else {
        lua_pushlstring(L, b, l);
    }
    return 1;
}

/*
** Check whether 'status' signals a syntax error and the error
** message at the top of the stack ends with the above mark for
** incomplete statements.
*/
static int incomplete(lua_State *L, int status)
{
    if (status == LUA_ERRSYNTAX) {
        size_t lmsg;
        const char *msg = lua_tolstring(L, -1, &lmsg);
        if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0) {
            lua_pop(L, 1);
            return 1;
        }
    }
    return 0;  /* else... */
}

/*
** Read multiple lines until a complete Lua statement
*/
static int multiline(LuaThreadState *lts)
{
    lua_State *L = lts->L;
    size_t len;
    const char *line = lua_tolstring(L, 1, &len);  /* get what it has */
    int status = luaL_loadbuffer(L, line, len, "=stdin");  /* try it */
    if (!incomplete(L, status) || !pushline(lts)) {
        return status;  /* cannot or should not try to add continuation line */
    }
    lua_pushliteral(L, "\n");  /* add newline... */
    lua_insert(L, -2);  /* ...between the two lines */
    lua_concat(L, 3);  /* join them */
    return 0;
}

/*
Print (using piworld's add_message) any values on the stack
*/
static void l_print(LuaThreadState *lts)
{
    lua_State *L = lts->L;
    int n = lua_gettop(L);
    if (n > 0) {  /* any result to be printed? */
      luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
      lua_getglobal(L, "tostring");
      for (int i=1; i<=n; i++) {
          const char *s;
          size_t l;
          lua_pushvalue(L, -1);  /* function to be called */
          lua_pushvalue(L, i);   /* value to print */
          lua_call(L, 1, 1);
          s = lua_tolstring(L, -1, &l);  /* get result */
          if (s == NULL) {
              printf("'tostring' must return a string to 'print'\n");
              break;
          }
          add_message(lts->player_id, s);

          lua_pop(L, 1);  /* pop result */
        }
    }
}

/*
** Message handler used to run all chunks
*/
static int msghandler(lua_State *L)
{
    const char *msg = lua_tostring(L, 1);
    if (msg == NULL) {  /* is error object not a string? */
        if (luaL_callmeta(L, 1, "__tostring") && /* does it have a metamethod */
            lua_type(L, -1) == LUA_TSTRING) {  /* that produces a string? */
            return 1;  /* that is the message */
        } else {
            msg = lua_pushfstring(L, "(error object is a %s value)",
                                     luaL_typename(L, 1));
        }
    }
    luaL_traceback(L, L, msg, 1);  /* append a standard traceback */
    return 1;  /* return the traceback */
}

/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack. It assumes that the error object
** is a string, as it was either generated by Lua or by 'msghandler'.
*/
static int report(LuaThreadState *lts, int status)
{
    lua_State *L = lts->L;
    if (status != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        add_message(lts->player_id, msg);
        lua_pop(L, 1);  /* remove message */
    }
    return status;
}

/*
** Interface to 'lua_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall(lua_State *L, int narg, int nres)
{
    int status;
    int base = lua_gettop(L) - narg;  /* function index */
    lua_pushcfunction(L, msghandler);  /* push message handler */
    lua_insert(L, base);  /* put it under function and args */
    status = lua_pcall(L, narg, nres, base);
    lua_remove(L, base);  /* remove message handler from the stack */
    return status;
}

/*
** Read a line and try to load (compile) it first as an expression (by
** adding "return " in front of it) and second as a statement. Return
** the final status of load/call with the resulting function (if any)
** in the top of the stack.
*/
static int loadline(LuaThreadState *lts)
{
    lua_State *L = lts->L;
    int status;
    lua_settop(L, 0);
    if (!pushline(lts)) {
        return -1;  /* no input */
    }
    if ((status = addreturn(L)) != LUA_OK) { /* 'return ...' did not work? */
        status = multiline(lts);  /* try as command, maybe with continuation
                                     lines */
    }
    lua_remove(L, 1);  /* remove line from the stack */
    lua_assert(lua_gettop(L) == 1);
    return status;
}

void process_lua_code_line(LuaThreadState *lts)
{
    lua_State *L = lts->L;
    int status;

    status = loadline(lts);

    if (status == LUA_OK) {
        status = docall(L, 0, LUA_MULTRET);
    }
    if (status == LUA_OK) {
        l_print(lts);
    } else {
        report(lts, status);
    }
}

int pwlua_thread_run_protected_mode(lua_State *L)
{
    LuaThreadState *lts = (LuaThreadState *)lua_touserdata(L, 1);

    do {
        process_lua_code_line(lts);
        if (!lts->is_shell) {
            break;
        }
        lts->state = WAITING;
        cnd_wait(&lts->cnd, &lts->mtx);
    } while (!lts->terminate);

    return 0;
}

int pwlua_thread_run(LuaThreadState *lts)
{
    lua_State *L = luaL_newstate();
    lts->L = L;

    luaL_openlibs(L);

#ifdef RASPI
    luaopen_GPIO(L);
#endif

    lua_pushnumber(L, lts->player_id);
    lua_setglobal(L, "player_id");

    pwlua_api_add_functions(L);
    pwlua_api_add_constants(L);

    lua_pushcfunction(L, &pwlua_thread_run_protected_mode);
    lua_pushlightuserdata(L, lts);
    int status = lua_pcall(L, 1, 1, 0);

    lua_close(L);
    lts->state = STOPPED;
    return status;
}

/******************************************************************************
** Lua REPL adapted from Lua 5.3 under the following license.
* Copyright (C) 1994-2018 Lua.org, PUC-Rio.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

