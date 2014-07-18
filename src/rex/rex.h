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

/* Rex: a portable socket library */

#ifndef _H_PROXYIO_REX_
#define _H_PROXYIO_REX_

#include <inttypes.h>

struct rex_sock;

struct rex_iov {
	char *iov_base;
	int iov_len;
};

#if defined MS_WINDOWS
# include "rex_win.h"
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) \
	|| defined(__NetBSD__) || defined(linux)
# include "rex_posix.h"
#endif

/* the following af_family are supported */

enum {
	REX_AF_LOCAL   =   1,     /* localhost communication */
	REX_AF_TCP,               /* tcp4 communication */
	REX_AF_TCP6,              /* tcp6 communication */
};

int rex_sock_init (struct rex_sock *rs, int family /* AF_LOCAL|AF_TCP|AF_TCP6 */);
int rex_sock_destroy (struct rex_sock *rs);

/* listen on the sockaddr */
int rex_sock_listen (struct rex_sock *rs, const char *sockaddr);

/* accept one new connection for the initialized new sock */
int rex_sock_accept (struct rex_sock *rs, struct rex_sock *new);

/* connect to remote host */
int rex_sock_connect (struct rex_sock *rs, const char *sockaddr);

int rex_sock_sendv (struct rex_sock *rs, struct rex_iov *iov, int n);

static inline int rex_sock_send (struct rex_sock *rs, char *buff, int size)
{
	struct rex_iov iov = {
		.iov_base = buff,
		.iov_len = size,
	};
	return rex_sock_sendv (rs, &iov, 1);
}

int rex_sock_recvv (struct rex_sock *rs, struct rex_iov *iov, int n);

static inline int rex_sock_recv (struct rex_sock *rs, char *buff, int size)
{
	struct rex_iov iov = {
		.iov_base = buff,
		.iov_len = size,
	};
	return rex_sock_recvv (rs, &iov, 1);
}

/* Following gs[etsockopt] options are supported by rex library */
enum {
	REX_SO_BACKLOG    =   0,
	REX_SO_LINGER,
	REX_SO_SNDBUF,
	REX_SO_RCVBUF,
	REX_SO_NOBLOCK,
	REX_SO_NODELAY,
	REX_SO_RCVTIMEO,
	REX_SO_SNDTIMEO,
	REX_SO_REUSEADDR,
};
int rex_sock_setopt (struct rex_sock *rs, int opt, void *optval, int optlen);
int rex_sock_getopt (struct rex_sock *rs, int opt, void *optval, int *optlen);



#endif
