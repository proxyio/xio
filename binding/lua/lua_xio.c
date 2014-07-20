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

static int lua_ubuf_free (lua_State *L)
{
	char *ubuf = * (char **) lua_touserdata (L, 1);

	if (ubuf) {
		ubuf_free (ubuf);
	}
	return 0;
}

static int lua_sp_endpoint (lua_State *L)
{
	int sp_family = luaL_checkint (L, 1);
	int sp_type = luaL_checkint (L, 2);
	lua_pushnumber (L, sp_endpoint (sp_family, sp_type));
	return 1;
}

static int lua_sp_close (lua_State *L)
{
	int eid = luaL_checkint (L, 1);
	lua_pushnumber (L, sp_close (eid));
	return 1;
}

static int lua_sp_send (lua_State *L)
{
	int rc;
	int eid = luaL_checkint (L, 1);
	char *ubuf;
	const char *msg;
	char *hdr;

	lua_getfield (L, 2, "data");
	lua_getfield (L, 2, "hdr");
	msg = lua_tostring (L, -2);
	hdr = lua_touserdata (L, -1);
	if (hdr)
		hdr = * (char **) hdr;

	ubuf = ubuf_alloc (strlen (msg) + 1);
	memcpy (ubuf, msg, ubuf_len (ubuf));
	ubuf [ubuf_len (ubuf) - 1] = 0;

	if (hdr)
		ubufctl (hdr, SCOPY, ubuf);
	if ((rc = sp_send (eid, ubuf)))
		ubuf_free (ubuf);
	lua_pushnumber (L, rc);
	return 1;
}

static int lua_sp_recv (lua_State *L)
{
	int rc = 0;
	int fd = luaL_checkint (L, 1);
	char *ud = 0;
	char *ubuf = 0;

	if ((rc = sp_recv (fd, &ubuf)) != 0) {
		lua_pushnumber (L, rc);
		lua_pushstring (L, "error");
		return 2;
	}
	lua_pushnumber (L, rc);
	lua_newtable (L);

	lua_pushstring (L, ubuf);
	lua_setfield (L, -2, "data");

	ud = lua_newuserdata (L, sizeof (ubuf));
	* (char **) ud = ubuf;
	luaL_getmetatable (L, "ubuf_metatable");
	lua_setmetatable (L, -2);

	lua_setfield (L, -2, "hdr");
	return 2;
}

static int lua_sp_add (lua_State *L)
{
	int eid = luaL_checkint (L, 1);
	int sockfd = luaL_checkint (L, 2);
	lua_pushnumber (L, sp_add (eid, sockfd));
	return 1;
}

static int lua_sp_rm (lua_State *L)
{
	int eid = luaL_checkint (L, 1);
	int sockfd = luaL_checkint (L, 2);
	lua_pushnumber (L, sp_rm (eid, sockfd));
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
	lua_pushnumber (L, sp_connect (eid, sockaddr));
	return 1;
}


static int lua_sp_listen (lua_State *L)
{
	int eid = luaL_checkint (L, 1);
	const char *sockaddr = luaL_checkstring (L, 2);
	lua_pushnumber (L, sp_listen (eid, sockaddr));
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

	luaL_newmetatable (L, "ubuf_metatable");
	lua_pushcfunction (L, lua_ubuf_free);
	lua_setfield (L, -2, "__gc");
	lua_pop (L, 1);
	
	return 0;
}
