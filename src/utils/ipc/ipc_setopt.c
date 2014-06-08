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


#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "ipc.h"
#include "../list.h"

static int set_linger(int fd, void *optval, int optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int set_sndbuf(int fd, void *optval, int optlen)
{
	int rc;
	int buf = *(int *)optval;

	if (buf <= 0)
		buf = TP_RCVBUFLEN;
	rc = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));
	return rc;
}

static int set_rcvbuf(int fd, void *optval, int optlen)
{
	int rc;
	int buf = *(int *)optval;

	if (buf <= 0)
		buf = TP_SNDBUFLEN;
	rc = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buf, sizeof(buf));
	return rc;
}

static int set_noblock(int fd, void *optval, int optlen)
{
	int rc;
	int flags;
	int on = *((int *)optval);

	if ((flags = fcntl(fd, F_GETFL, 0)) < 0)
		flags = 0;
	flags = on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
	rc = fcntl(fd, F_SETFL, flags);
	return rc;
}

static int set_nodelay(int fd, void *optval, int optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int set_sndtimeo(int fd, void *optval, int optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int set_rcvtimeo(int fd, void *optval, int optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int set_reuseaddr(int fd, void *optval, int optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static const tp_setopt setopt_vfptr[] = {
	0,
	set_linger,
	set_sndbuf,
	set_rcvbuf,
	set_noblock,
	set_nodelay,
	set_rcvtimeo,
	set_sndtimeo,
	set_reuseaddr,
};

int ipc_setopt(int fd, int opt, void *optval, int optlen)
{
	int rc;

	if (opt >= NELEM(setopt_vfptr, tp_setopt) || !setopt_vfptr[opt]) {
		errno = EINVAL;
		return -1;
	}
	rc = setopt_vfptr[opt] (fd, optval, optlen);
	return rc;
}
