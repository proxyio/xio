/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#include <lua.h>
#include <xio/socket.h>
#include <xio/poll.h>
#include <xio/sp.h>
#include <xio/sp_reqrep.h>
#include <string.h>
#include <lauxlib.h>
#include <utils/base.h>

static int lua_sp_endpoint (lua_State *L)
{
	int sp_family = luaL_checkint (L, 1);
	int sp_type = luaL_checkint (L, 2);
	lua_pushnumber (L, sp_endpoint (sp_family, sp_type) );
	return 1;
}

static int lua_sp_close (lua_State *L)
{
	int eid = luaL_checkint (L, 1);
	lua_pushnumber (L, sp_close (eid) );
	return 1;
}

static int lua_sp_send (lua_State *L)
{
	int eid = luaL_checkint (L, 1);
	char *ubuf = (char *) lua_touserdata (L, 2);
	lua_pushnumber (L, sp_send (eid, ubuf) );
	return 1;
}

static int lua_sp_recv (lua_State *L)
{
	char *ubuf = 0;
	int fd = luaL_checkint (L, 1);
	lua_pushnumber (L, sp_recv (fd, &ubuf) );
	lua_pushlightuserdata (L, ubuf);
	return 2;
}

static int lua_sp_add (lua_State *L)
{
	int eid = luaL_checkint (L, 1);
	int sockfd = luaL_checkint (L, 2);
	lua_pushnumber (L, sp_add (eid, sockfd) );
	return 1;
}

static int lua_sp_rm (lua_State *L)
{
	int eid = luaL_checkint (L, 1);
	int sockfd = luaL_checkint (L, 2);
	lua_pushnumber (L, sp_rm (eid, sockfd) );
	return 1;
}

static int lua_sp_setopt (lua_State *L)
{
	return 0;
}


static int lua_sp_getopt (lua_State *L)
{
	return 0;
}

static int lua_sp_connect (lua_State *L)
{
	int eid = luaL_checkint (L, 1);
	const char *sockaddr = luaL_checkstring (L, 2);
	lua_pushnumber (L, sp_connect (eid, sockaddr) );
	return 1;
}


static int lua_sp_listen (lua_State *L)
{
	int eid = luaL_checkint (L, 1);
	const char *sockaddr = luaL_checkstring (L, 2);
	lua_pushnumber (L, sp_listen (eid, sockaddr) );
	return 1;
}


static const struct luaL_reg func_symbols [] = {
	{"sp_endpoint",     lua_sp_endpoint},
	{"sp_close",        lua_sp_close},
	{"sp_send",         lua_sp_send},
	{"sp_recv",         lua_sp_recv},
	{"sp_add",          lua_sp_add},
	{"sp_rm",           lua_sp_rm},
	{"sp_setopt",       lua_sp_setopt},
	{"sp_getopt",       lua_sp_getopt},
	{"sp_connect",      lua_sp_connect},
	{"sp_listen",       lua_sp_listen},
};


struct sym_kv {
	const char name[32];
	int value;
};

static const struct sym_kv const_symbols [] = {
	{"SP_REQREP",    SP_REQREP},
	{"SP_BUS",       SP_BUS},
	{"SP_PUBSUB",    SP_PUBSUB},
	{"SP_REQ",       SP_REQ},
	{"SP_REP",       SP_REP},
	{"SP_PROXY",     SP_PROXY},
};

LUA_API int luaopen_xio (lua_State *L)
{
	int i;
	for (i = 0; i < NELEM (func_symbols, struct luaL_reg); i++) {
		lua_pushcfunction (L, func_symbols[i].func);
		lua_setglobal (L, func_symbols[i].name);
	}
	for (i = 0; i < NELEM (const_symbols, struct sym_kv); i++) {
		lua_pushnumber (L, const_symbols[i].value);
		lua_setglobal (L, const_symbols[i].name);
	}
	return 0;
}
