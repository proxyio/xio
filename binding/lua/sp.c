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

#include <xio/sp.h>
#include <xio/sp_reqrep.h>
#include <string.h>
#include <lauxlib.h>
#include <utils/base.h>
#include "xio_if.h"


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
    char *ubuf = 0;
    int fd = luaL_checkint(L, 1);
    lua_pushnumber(L, sp_recv(fd, &ubuf));
    lua_pushlightuserdata(L, ubuf);
    return 2;
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

static int lsp_connect(lua_State *L) {
    int eid = luaL_checkint(L, 1);
    const char *sockaddr = luaL_checkstring(L, 2);
    lua_pushnumber(L, sp_connect(eid, sockaddr));
    return 1;
}


static int lsp_listen(lua_State *L) {
    int eid = luaL_checkint(L, 1);
    const char *sockaddr = luaL_checkstring(L, 2);
    lua_pushnumber(L, sp_listen(eid, sockaddr));
    return 1;
}

static const struct luaL_reg __sp_apis [] = {
    {"sp_endpoint",     lsp_endpoint},
    {"sp_close",        lsp_close},
    {"sp_send",         lsp_send},
    {"sp_recv",         lsp_recv},
    {"sp_add",          lsp_add},
    {"sp_rm",           lsp_rm},
    {"sp_setopt",       lsp_setopt},
    {"sp_getopt",       lsp_getopt},
    {"sp_connect",      lsp_connect},
    {"sp_listen",       lsp_listen},
};

const struct luaL_reg *sp_apis = __sp_apis;
int sp_apis_num = NELEM(__sp_apis, struct luaL_reg);


static const struct xsymbol __sp_consts [] = {
    {"SP_REQREP",    SP_REQREP},
    {"SP_BUS",       SP_BUS},
    {"SP_PUBSUB",    SP_PUBSUB},
    {"SP_REQ",       SP_REQ},
    {"SP_REP",       SP_REP},
    {"SP_PROXY",     SP_PROXY},
};
const struct xsymbol *sp_consts = __sp_consts;
int sp_consts_num = NELEM(__sp_consts, struct xsymbol);
