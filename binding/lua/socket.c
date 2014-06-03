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
#include <utils/base.h>
#include "xio_if.h"

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

static const struct luaL_reg __socket_apis [] = {
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
};
const struct luaL_reg *socket_apis = __socket_apis;
int socket_apis_num = NELEM(__socket_apis, struct luaL_reg);

static const struct xsymbol __socket_consts [] = {
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
};
const struct xsymbol *socket_consts = __socket_consts;
int socket_consts_num = NELEM(__socket_consts, struct xsymbol);
