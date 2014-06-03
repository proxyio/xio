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

#include <string.h>
#include <lauxlib.h>
#include <utils/base.h>
#include "xio_if.h"


#define lua_registerFunc(L, reg) do {		\
	lua_pushcfunction(L, (reg).func);	\
	lua_setglobal(L, (reg).name);		\
    } while (0)

#define lua_registerConst(L, cst) do {		\
	lua_pushnumber(L, (cst).value );	\
	lua_setglobal(L, (cst).name );		\
    } while (0)


extern const struct luaL_reg *socket_apis;
extern int socket_apis_num;
extern const struct luaL_reg *poll_apis;
extern int poll_apis_num;
extern const struct luaL_reg *sp_apis;
extern int sp_apis_num;

extern const struct xsymbol *socket_consts;
extern int socket_consts_num;
extern const struct xsymbol *poll_consts;
extern int poll_consts_num;
extern const struct xsymbol *sp_consts;
extern int sp_consts_num;

int luaL_xio (lua_State *L) {
    int i;
    for (i = 0; i < socket_apis_num; i++) {
	lua_registerFunc(L, socket_apis[i]);
    }
    for (i = 0; i < poll_apis_num; i++) {
	lua_registerFunc(L, poll_apis[i]);
    }
    for (i = 0; i < sp_apis_num; i++) {
	lua_registerFunc(L, sp_apis[i]);
    }
    for (i = 0; i < socket_consts_num; i++) {
	lua_registerConst(L, socket_consts[i]);
    }
    for (i = 0; i < poll_consts_num; i++) {
	lua_registerConst(L, poll_consts[i]);
    }
    for (i = 0; i < sp_consts_num; i++) {
	lua_registerConst(L, sp_consts[i]);
    }
    return 0;
}
