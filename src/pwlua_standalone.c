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
#include <ctype.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "linenoise.h"
#include "pw.h"
#include "pwlua.h"
#include "pwlua_api.h"
#ifdef RASPI
#include "RPi_GPIO_Lua_module.h"
#endif

static char history_path[MAX_PATH_LENGTH];

/* mark in error messages for incomplete statements */
#define EOFMARK		"<eof>"
#define marklen		(sizeof(EOFMARK)/sizeof(char) - 1)

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
** Prompt the user, read a line, and push it into the Lua stack.
*/
static int pushline (lua_State *L, int firstline)
{
    size_t l;
    char *b;
    b = linenoise("lua> ");
    if (b == NULL) {
        return 0;  /* no input (prompt will be popped by caller) */
    }
    linenoiseHistoryAdd(b); /* Add to the history. */
    linenoiseHistorySave(history_path); /* Save the history on disk. */
    l = strlen(b);
    if (l > 0 && b[l-1] == '\n') {  /* line ends with newline? */
        b[--l] = '\0';  /* remove it */
    }
    if (firstline && b[0] == '=') {  /* for compatibility with 5.2, ... */
        lua_pushfstring(L, "return %s", b + 1);  /* change '=' to 'return' */
    } else {
        lua_pushlstring(L, b, l);
    }
    linenoiseFree(b);
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
static int multiline(lua_State *L)
{
    size_t len;
    const char *line = lua_tolstring(L, 1, &len);  /* get what it has */
    int status = luaL_loadbuffer(L, line, len, "=stdin");  /* try it */
    if (!incomplete(L, status) || !pushline(L, 0)) {
        return status;  /* cannot or should not try to add continuation line */
    }
    lua_pushliteral(L, "\n");  /* add newline... */
    lua_insert(L, -2);  /* ...between the two lines */
    lua_concat(L, 3);  /* join them */
    return 0;
}

/*
** Prints (calling the Lua 'print' function) any values on the stack
*/
static void l_print(lua_State *L)
{
    int n = lua_gettop(L);
    if (n > 0) {  /* any result to be printed? */
        luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
        lua_getglobal(L, "print");
        lua_insert(L, 1);
        if (lua_pcall(L, n, 0, 0) != LUA_OK) {
            printf("%s\n", lua_pushfstring(L, "error calling 'print' (%s)",
                                           lua_tostring(L, -1)));
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
static int report (lua_State *L, int status)
{
    if (status != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        printf("%s\n", msg);
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
static int loadline(lua_State *L)
{
    int status;
    lua_settop(L, 0);
    if (!pushline(L, 1)) {
        return -1;  /* no input */
    }
    if ((status = addreturn(L)) != LUA_OK) { /* 'return ...' did not work? */
        status = multiline(L);  /* try as command, maybe with continuation
                                   lines */
    }
    lua_remove(L, 1);  /* remove line from the stack */
    lua_assert(lua_gettop(L) == 1);
    return status;
}

/*
** Do the REPL: repeatedly read (load) a line, evaluate (call) it, and
** print any results.
*/
static void doREPL (lua_State *L)
{
    int status;
    while ((status = loadline(L)) != -1) {
        if (status == LUA_OK)
            status = docall(L, 0, LUA_MULTRET);
        if (status == LUA_OK) l_print(L);
        else report(L, status);
    }
    lua_settop(L, 0);  /* clear stack */
    printf("\n");
}

void pwlua_standalone_REPL(void)
{
    snprintf(history_path, MAX_PATH_LENGTH, "%s/lua-standalone.history",
             config->path);
    linenoiseHistoryLoad(history_path);
    lua_State *L = luaL_newstate();

    luaL_openlibs(L);

#ifdef RASPI
    luaopen_GPIO(L);
#endif

    doREPL(L);

    lua_close(L);
}

