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

#ifndef _H_PROXYIO_TCP_INTERN_
#define _H_PROXYIO_TCP_INTERN_

#include <rex/rex.h>
#include "../sockbase.h"

struct sio_sock {
	struct sockbase base;
	struct ev_fd et;
	struct rex_sock s;
	struct bio in;
	struct io ops;
	struct list_head un_head;
	int un_snd;                  /* the number of un_iov */
	struct rex_iov un_iov[100];
};

int sio_setopt (struct sockbase *sb, int opt, void *optval, int optlen);
int sio_getopt (struct sockbase *sb, int opt, void *optval, int *optlen);

extern struct sockbase_vfptr tcp_listener_vfptr;
extern struct sockbase_vfptr tcp_connector_vfptr;
extern struct sockbase_vfptr ipc_listener_vfptr;
extern struct sockbase_vfptr ipc_connector_vfptr;

#endif
