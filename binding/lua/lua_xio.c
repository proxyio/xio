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

#include <xio/socket.h>
#include <xio/poll.h>
#include <xio/sp.h>
#include <xio/sp_reqrep.h>
#include <string.h>
#include <lauxlib.h>
#include <libc/base.h>
#include "lua_xio.h"

static int lxallocubuf (lua_State *L) {
    const char *str = luaL_checkstring(L, 1);
    char *ubuf = xallocubuf(strlen(str));
    strncpy(ubuf, str, strlen(ubuf));
    lua_pushlightuserdata(L, ubuf);
    return 1;
}

static int lxubuflen (lua_State *L) {
    char *ubuf = (char *)lua_touserdata(L, 1);
    luaL_argcheck(L, ubuf != NULL, 1, "`ubuf' expected");
    lua_pushnumber(L, xubuflen(ubuf));
    return 1;
}

static int lxfreeubuf (lua_State *L) {
    char *ubuf = (char *)lua_touserdata(L, 1);
    xfreeubuf(ubuf);
    return 1;
}

static int lxsocket(lua_State *L) {
    int pf = luaL_checkint(L, 1);
    int socktype = luaL_checkint(L, 2);
    int fd = xsocket(pf, socktype);
    lua_pushnumber(L, fd);
    return 1;
}

static int lxbind(lua_State *L) {
    int fd = luaL_checkint(L, 1);
    const char *addr = luaL_checkstring(L, 2);
    lua_pushnumber(L, xbind(fd, addr));
    return 1;
}

static int lxlisten(lua_State *L) {
    const char *addr = luaL_checkstring(L, 1);
    lua_pushnumber(L, xlisten(addr));
    return 1;
}

static int lxaccept(lua_State *L) {
    int fd = luaL_checkint(L, 1);
    lua_pushnumber(L, xaccept(fd));
    return 1;
}

static int lxconnect(lua_State *L) {
    const char *addr = luaL_checkstring(L, 1);
    lua_pushnumber(L, xconnect(addr));
    return 1;
}

static int lxrecv(lua_State *L) {
    char *ubuf = 0;
    int fd = luaL_checkint(L, 1);
    lua_pushnumber(L, xrecv(fd, &ubuf));
    lua_pushlightuserdata(L, ubuf);
    return 2;
}

static int lxsend(lua_State *L) {
    int fd = luaL_checkint(L, 1);
    char *ubuf = (char *)lua_touserdata(L, 2);
    lua_pushnumber(L, xsend(fd, ubuf));
    return 1;
}

static int lxclose(lua_State *L) {
    int fd = luaL_checkint(L, 1);
    lua_pushnumber(L, xclose(fd));
    return 1;
}

static int lxsetopt(lua_State *L) {
    return 0;
}

static int lxgetopt(lua_State *L) {
    return 0;
}

static int lsp_endpoint(lua_State *L) {
    int sp_family = luaL_checkint(L, 1);
    int sp_type = luaL_checkint(L, 2);
    lua_pushnumber(L, sp_endpoint(sp_family, sp_type));
    return 1;
}

static int lsp_close(lua_State *L) {
    int eid = luaL_checkint(L, 1);
    lua_pushnumber(L, sp_close(eid));
    return 1;
}

static int lsp_send(lua_State *L) {
    int eid = luaL_checkint(L, 1);
    char *ubuf = (char *)lua_touserdata(L, 2);
    lua_pushnumber(L, sp_send(eid, ubuf));
    return 1;
}

static int lsp_recv(lua_State *L) {
    return 0;
}

static int lsp_add(lua_State *L) {
    int eid = luaL_checkint(L, 1);
    int sockfd = luaL_checkint(L, 2);
    lua_pushnumber(L, sp_add(eid, sockfd));
    return 1;
}

static int lsp_rm(lua_State *L) {
    int eid = luaL_checkint(L, 1);
    int sockfd = luaL_checkint(L, 2);
    lua_pushnumber(L, sp_rm(eid, sockfd));
    return 1;
}

static int lsp_setopt(lua_State *L) {
    return 0;
}


static int lsp_getopt(lua_State *L) {
    return 0;
}


static const struct luaL_reg xio_apis [] = {
    {"xallocubuf",      lxallocubuf},
    {"xubuflen",        lxubuflen},
    {"xfreeubuf",       lxfreeubuf},
    {"xsocket",         lxsocket},
    {"xbind",           lxbind},
    {"xlisten",         lxlisten},
    {"xconnect",        lxconnect},
    {"xaccept",         lxaccept},
    {"xrecv",           lxrecv},
    {"xsend",           lxsend},
    {"xclose",          lxclose},
    {"xsetopt",         lxsetopt},
    {"xgetopt",         lxgetopt},
    {"sp_endpoint",     lsp_endpoint},
    {"sp_close",        lsp_close},
    {"sp_send",         lsp_send},
    {"sp_recv",         lsp_recv},
    {"sp_add",          lsp_add},
    {"sp_rm",           lsp_rm},
    {"sp_setopt",       lsp_setopt},
    {"sp_getopt",       lsp_getopt},
};


typedef struct {
    const char *name;
    int value;
} XIOCONST;

static XIOCONST xio_consts [] = {
    {"XPOLLIN",      XPOLLIN},
    {"XPOLLOUT",     XPOLLOUT},
    {"XPOLLERR",     XPOLLERR},

    {"XPOLL_ADD",    XPOLL_ADD},
    {"XPOLL_DEL",    XPOLL_DEL},
    {"XPOLL_MOD",    XPOLL_MOD},

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

    {"SP_REQREP",    SP_REQREP},
    {"SP_BUS",       SP_BUS},
    {"SP_PUBSUB",    SP_PUBSUB},

    {"SP_REQ",       SP_REQ},
    {"SP_REP",       SP_REP},
    {"SP_PROXY",     SP_PROXY},
};


#define lua_registerFunc(L, reg) do {		\
	lua_pushcfunction(L, reg.func);		\
	lua_setglobal(L, reg.name);		\
    } while (0)

#define lua_registerConst(L, cst) do {		\
	lua_pushnumber(L, cst.value );		\
	lua_setglobal(L, cst.name );		\
    } while (0)

int luaopen_xio (lua_State *L) {
    int i;
    for (i = 0; i < NELEM(xio_apis, luaL_reg); i++) {
	lua_registerFunc(L, xio_apis[i]);
    }
    for (i = 0; i < NELEM(xio_consts, XIOCONST); i++) {
	lua_registerConst(L, xio_consts[i]);
    }
    return 1;
}
