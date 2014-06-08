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

#ifndef _XIO_POLL_
#define _XIO_POLL_

#include <xio/cplusplus_define.h>

#define XPOLLIN   1
#define XPOLLOUT  2
#define XPOLLERR  4

int xselect (int events, int nin, int *in_set, int nout, int *out_set);

struct poll_ent {
	int fd;
	void *self;
	int events;
	int happened;
};

int xpoll_create();
int xpoll_close (int pollid);

#define XPOLL_ADD 1
#define XPOLL_DEL 2
#define XPOLL_MOD 3
int xpoll_ctl (int pollid, int op, struct poll_ent *ent);
int xpoll_wait (int pollid, struct poll_ent *ent, int n, int timeout);

#include <xio/cplusplus_endif.h>
#endif
