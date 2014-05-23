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
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "tcp.h"
#include "../list.h"

static int get_linger(int fd, void *optval, int *optlen) {
    errno = EOPNOTSUPP;
    return -1;
}

static int get_sndbuf(int fd, void *optval, int *optlen) {
    int rc;
    int buf = 0;
    socklen_t koptlen = sizeof(buf);

    rc = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buf, &koptlen);
    if (rc == 0)
	*(int *)optval = buf;
    return rc;
}

static int get_rcvbuf(int fd, void *optval, int *optlen) {
    int rc;
    int buf = 0;
    socklen_t koptlen = sizeof(buf);

    rc = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buf, &koptlen);
    if (rc == 0)
	*(int *)optval = buf;
    return rc;
}

static int get_noblock(int fd, void *optval, int *optlen) {
    int flags;

    if ((flags = fcntl(fd, F_GETFL, 0)) < 0)
	flags = 0;
    *(int *)optval = (flags & O_NONBLOCK) ? true : false;
    return 0;
}

static int get_nodelay(int fd, void *optval, int *optlen) {
    errno = EOPNOTSUPP;
    return -1;
}

static int get_sndtimeo(int fd, void *optval, int *optlen) {
    int rc;
    int to = *(int *)optval;
    struct timeval tv = {
	.tv_sec = to / 1000,
	.tv_usec = (to % 1000) * 1000,
    };
    socklen_t koptlen = sizeof(tv);

    rc = getsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, &koptlen);
    if (rc == 0)
	*(int *)optval = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return rc;
}

static int get_rcvtimeo(int fd, void *optval, int *optlen) {
    int rc;
    int to = *(int *)optval;
    struct timeval tv = {
	.tv_sec = to / 1000,
	.tv_usec = (to % 1000) * 1000,
    };
    socklen_t koptlen = sizeof(tv);

    rc = getsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, &koptlen);
    if (rc == 0)
	*(int *)optval = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return rc;
}

static int get_reuseaddr(int fd, void *optval, int *optlen) {
    errno = EOPNOTSUPP;
    return -1;
}

static const tp_getopt getopt_vfptr[] = {
    0,
    get_linger,
    get_sndbuf,
    get_rcvbuf,
    get_noblock,
    get_nodelay,
    get_rcvtimeo,
    get_sndtimeo,
    get_reuseaddr,
};

int tcp_getopt(int fd, int opt, void *optval, int *optlen) {
    int rc;

    if (opt >= NELEM(getopt_vfptr, tp_getopt) || !getopt_vfptr[opt]) {
	errno = EINVAL;
	return -1;
    }
    rc = getopt_vfptr[opt] (fd, optval, optlen);
    return rc;
}
