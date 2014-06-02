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

#include <xio/poll.h>
#include <string.h>
#include <lauxlib.h>
#include <utils/base.h>
#include "lua_xio.h"

static const struct luaL_reg __poll_apis [] = {
};
const struct luaL_reg *poll_apis = __poll_apis;
int poll_apis_num = NELEM(__poll_apis, struct luaL_reg);


static const struct xsymbol __poll_consts [] = {
    {"XPOLLIN",      XPOLLIN},
    {"XPOLLOUT",     XPOLLOUT},
    {"XPOLLERR",     XPOLLERR},
    {"XPOLL_ADD",    XPOLL_ADD},
    {"XPOLL_DEL",    XPOLL_DEL},
    {"XPOLL_MOD",    XPOLL_MOD},
};
const struct xsymbol *poll_consts = __poll_consts;
int poll_consts_num = NELEM(__poll_consts, struct xsymbol);
