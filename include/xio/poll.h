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

#ifndef _H_PROXYIO_POLL_
#define _H_PROXYIO_POLL_

#include <xio/cplusplus_define.h>

enum {
	XPOLLIN   =  1,
	XPOLLOUT  =  2,
	XPOLLERR  =  4,
};

int xselect (int events, int nin, int *in_set, int nout, int *out_set);

struct poll_fd {
	int fd;
	void *hndl;
	int events;
	int happened;
};

int xpoll_create();
int xpoll_close (int pollid);

enum {
	XPOLL_ADD = 1,
	XPOLL_DEL = 2,
	XPOLL_MOD = 3,
};

int xpoll_ctl (int pollid, int op, struct poll_fd *pollfd);
int xpoll_wait (int pollid, struct poll_fd *fds, int n, int timeout);

#include <xio/cplusplus_endif.h>
#endif
