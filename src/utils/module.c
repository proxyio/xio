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

#include "base.h"

int gettid()
{
	return syscall (__NR_gettid);
}

extern void xsocket_module_init();
extern void transport_module_init();
extern void xpoll_module_init();
extern void sp_module_init();

void __attribute__ ( (constructor) ) __modules_init (void)
{
	transport_module_init();
	xsocket_module_init();
	xpoll_module_init();
	sp_module_init();
}


extern void xsocket_module_exit();
extern void transport_module_exit();
extern void sp_module_exit();
extern void xpoll_module_exit();

void __attribute__ ( (destructor) ) __modules_exit (void)
{
	sp_module_exit();
	xpoll_module_exit();
	xsocket_module_exit();
	transport_module_exit();
}
