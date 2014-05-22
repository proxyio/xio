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

#include <lauxlib.h>
#include <socket/xmsg.h>
#include "lxmsg.h"


static int lxmsg_alloc (lua_State *L) {
    const char *ubuf = luaL_checkstring(L, 1);
    struct xmsg **msg = (struct xmsg **)lua_newuserdata(L, 1);
    *msg = xallocmsg(strlen(ubuf));
    strncpy((*msg)->vec.xiov_base, ubuf, strlen(ubuf));
    /* new userdatum is already on the stack */
    return 1;
}

static int lxmsg_size (lua_State *L) {
    struct xmsg **msg = (struct xmsg **)lua_touserdata(L, 1);
    luaL_argcheck(L, *msg != NULL, 1, "`xmsg' expected");
    lua_pushnumber(L, xmsglen(*msg));
    return 1;
}

static int lxmsg_free (lua_State *L) {
    struct xmsg **msg = (struct xmsg **)lua_touserdata(L, 1);
    xfreemsg(*msg);
    *msg = 0;
    return 1;
}

static const struct luaL_reg lxmsglib [] = {
    {"alloc", lxmsg_alloc},
    {"size", lxmsg_size},
    {"free", lxmsg_free},
    {NULL, NULL}
};


int lopen_xmsg (lua_State *L) {
    luaL_openlib(L, "xmsg", lxmsglib, 0);
    return 1;
}
