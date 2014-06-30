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

struct xsymbol {
	const char name[32];
	int value;
};

static int lua_ubuf_alloc (lua_State *L)
{
	const char *str = luaL_checkstring (L, 1);
	char *ubuf = ubuf_alloc (strlen (str) );
	strncpy (ubuf, str, strlen (ubuf) );
	lua_pushlightuserdata (L, ubuf);
	return 1;
}

static int lua_ubuf_len (lua_State *L)
{
	char *ubuf = (char *) lua_touserdata (L, 1);
	luaL_argcheck (L, ubuf != NULL, 1, "`ubuf' expected");
	lua_pushnumber (L, ubuf_len (ubuf) );
	return 1;
}

static int lua_ubuf_free (lua_State *L)
{
	char *ubuf = (char *) lua_touserdata (L, 1);
	ubuf_free (ubuf);
	return 1;
}

static int lua_xsocket (lua_State *L)
{
	int pf = luaL_checkint (L, 1);
	int socktype = luaL_checkint (L, 2);
	int fd = xsocket (pf, socktype);
	lua_pushnumber (L, fd);
	return 1;
}

static int lua_xbind (lua_State *L)
{
	int fd = luaL_checkint (L, 1);
	const char *addr = luaL_checkstring (L, 2);
	lua_pushnumber (L, xbind (fd, addr) );
	return 1;
}

static int lua_xlisten (lua_State *L)
{
	const char *addr = luaL_checkstring (L, 1);
	lua_pushnumber (L, xlisten (addr) );
	return 1;
}

static int lua_xaccept (lua_State *L)
{
	int fd = luaL_checkint (L, 1);
	lua_pushnumber (L, xaccept (fd) );
	return 1;
}

static int lua_xconnect (lua_State *L)
{
	const char *addr = luaL_checkstring (L, 1);
	lua_pushnumber (L, xconnect (addr) );
	return 1;
}

static int lua_xrecv (lua_State *L)
{
	char *ubuf = 0;
	int fd = luaL_checkint (L, 1);
	lua_pushnumber (L, xrecv (fd, &ubuf) );
	lua_pushlightuserdata (L, ubuf);
	return 2;
}

static int lua_xsend (lua_State *L)
{
	int fd = luaL_checkint (L, 1);
	char *ubuf = (char *) lua_touserdata (L, 2);
	lua_pushnumber (L, xsend (fd, ubuf) );
	return 1;
}

static int lua_xclose (lua_State *L)
{
	int fd = luaL_checkint (L, 1);
	lua_pushnumber (L, xclose (fd) );
	return 1;
}

static int lua_xsetopt (lua_State *L)
{
	return 0;
}

static int lua_xgetopt (lua_State *L)
{
	return 0;
}



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


static const struct luaL_reg api_symbols [] = {
	{"ubuf_alloc",      lua_ubuf_alloc},
	{"ubuf_len",        lua_ubuf_len},
	{"ubuf_free",       lua_ubuf_free},
	{"xsocket",         lua_xsocket},
	{"xbind",           lua_xbind},
	{"xlisten",         lua_xlisten},
	{"xconnect",        lua_xconnect},
	{"xaccept",         lua_xaccept},
	{"xrecv",           lua_xrecv},
	{"xsend",           lua_xsend},
	{"xclose",          lua_xclose},
	{"xsetopt",         lua_xsetopt},
	{"xgetopt",         lua_xgetopt},

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


static const struct xsymbol const_symbols [] = {
	{"XPF_TCP",      XPF_TCP},
	{"XPF_IPC",      XPF_IPC},
	{"XPF_INPROC",   XPF_INPROC},
	{"XLISTENER",    XLISTENER},
	{"XCONNECTOR",   XCONNECTOR},
	{"XSOCKADDRLEN", XSOCKADDRLEN},

	{"XL_SOCKET",    XL_SOCKET},
	{"XNOBLOCK",     XNOBLOCK},
	{"XSNDWIN",      XSNDWIN},
	{"XRCVWIN",      XRCVWIN},
	{"XSNDBUF",      XSNDBUF},
	{"XRCVBUF",      XRCVBUF},
	{"XLINGER",      XLINGER},
	{"XSNDTIMEO",    XSNDTIMEO},
	{"XRCVTIMEO",    XRCVTIMEO},
	{"XRECONNECT",   XRECONNECT},
	{"XSOCKTYPE",    XSOCKTYPE},
	{"XSOCKPROTO",   XSOCKPROTO},
	{"XTRACEDEBUG",  XTRACEDEBUG},

	{"XPOLLIN",      XPOLLIN},
	{"XPOLLOUT",     XPOLLOUT},
	{"XPOLLERR",     XPOLLERR},
	{"XPOLL_ADD",    XPOLL_ADD},
	{"XPOLL_DEL",    XPOLL_DEL},
	{"XPOLL_MOD",    XPOLL_MOD},

	{"SP_REQREP",    SP_REQREP},
	{"SP_BUS",       SP_BUS},
	{"SP_PUBSUB",    SP_PUBSUB},
	{"SP_REQ",       SP_REQ},
	{"SP_REP",       SP_REP},
	{"SP_PROXY",     SP_PROXY},
};

#define LUA_REGISTER_GLOBAL_FUNCTION(L, reg) do {	\
	lua_pushcfunction(L, (reg).func);		\
	lua_setglobal(L, (reg).name);			\
    } while (0)

#define LUA_REGISTER_GLOBAL_CONSTANT(L, cst) do {	\
	lua_pushnumber(L, (cst).value );		\
	lua_setglobal(L, (cst).name );			\
    } while (0)

LUA_API int luaopen_xio (lua_State *L)
{
	int i;
	for (i = 0; i < NELEM (api_symbols, struct luaL_reg); i++)
		LUA_REGISTER_GLOBAL_FUNCTION (L, api_symbols[i]);
	for (i = 0; i < NELEM (const_symbols, struct xsymbol); i++)
		LUA_REGISTER_GLOBAL_CONSTANT (L, const_symbols[i]);
	return 0;
}
