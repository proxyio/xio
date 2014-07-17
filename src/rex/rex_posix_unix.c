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

static int rex_unix_init (struct rex_sock *rs)
{
	rs->ss_fd = -1;
	return 0;
}

static int rex_unix_destroy (struct rex_sock *rs)
{
	if (rs->ss_fd > 0) {
		close (rs->ss_fd);
		rs->ss_fd = -1;
		if (rs->ss_addr)
			unlink (rs->ss_addr);
	}
	rs->ss_family = 0;
	rs->ss_vfptr = 0;
	return 0;
}

/* listen on the sockaddr */
static int rex_unix_listen (struct rex_sock *rs, const char *sock)
{
	int fd;
	struct sockaddr_un addr;
	socklen_t addr_len = sizeof (addr);

	memset (&addr, 0, sizeof (addr));
#if defined(SOCK_CLOEXEC)
	fd = socket (AF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
#else
	fd = socket (AF_LOCAL, SOCK_STREAM, 0);
#endif
	if (fd < 0)
		return -1;
	addr.sun_family = AF_LOCAL;
	snprintf (addr.sun_path, sizeof (addr.sun_path), "%s", sock);
	unlink (addr.sun_path);
	if (bind (fd, (struct sockaddr *) &addr, addr_len) < 0
	    || listen (fd, REX_SODF_BACKLOG) < 0) {
		close (fd);
		return -1;
	}
	rs->ss_fd = fd;
	return 0;
}

/* accept one new connection for the initialized new sock */
static int rex_unix_accept (struct rex_sock *rs, struct rex_sock *new)
{
	struct sockaddr_un addr = {};
	socklen_t addrlen = sizeof (addr);
	int fd = accept (rs->ss_fd, (struct sockaddr *) &addr, &addrlen);

	if (fd < 0 &&
	    (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ||
	     errno == ECONNABORTED || errno == EMFILE) ) {
		errno = EAGAIN;
		return -1;
	}
	new->ss_family = rs->ss_family;
	new->ss_fd = fd;
	new->ss_vfptr = rs->ss_vfptr;
	return 0;
}

static int rex_unix_connect (struct rex_sock *rs, const char *peer)
{
	int fd;
	struct sockaddr_un addr;
	socklen_t addr_len = sizeof (addr);

	memset (&addr, 0, sizeof (addr));
	if ((fd = socket (AF_LOCAL, SOCK_STREAM, 0)) < 0)
		return -1;
	addr.sun_family = AF_LOCAL;
	snprintf (addr.sun_path, sizeof (addr.sun_path), "%s", peer);
	if (connect (fd, (struct sockaddr *) &addr, addr_len) < 0) {
		close (fd);
		return -1;
	}
	rs->ss_fd = fd;
	return 0;
}

static int rex_unix_send (struct rex_sock *rs, struct rex_iov *iov, int n)
{
	struct iovec diov[100];    /* local storage for performance */
	struct msghdr msg = {};
	int i;

#if defined MSG_MORE
#endif
	if (n > 100)
		n = 100;
	for (i = 0; i < n; i++) {
		diov[i].iov_base = iov[i].iov_base;
		diov[i].iov_len = iov[i].iov_len;
	}
	msg.msg_iov = diov;
	msg.msg_iovlen = n;
	return sendmsg (rs->ss_fd, &msg, 0);
}

static int rex_unix_recv (struct rex_sock *rs, struct rex_iov *iov, int n)
{
	struct iovec diov[100];
	struct msghdr msg = {};
	int i;

	if (n > 100)
		n = 100;
	for (i = 0; i < n; i++) {
		diov[i].iov_base = iov[i].iov_base;
		diov[i].iov_len = iov[i].iov_len;
	}
	msg.msg_iov = diov;
	msg.msg_iovlen = n;
	return recvmsg (rs->ss_fd, &msg, 0);
}

/* Following gs[etsockopt] options are supported by rex library
   REX_LINGER         
   REX_SNDBUF         
   REX_RCVBUF         
   REX_NOBLOCK        
   REX_NODELAY        
   REX_RCVTIMEO       
   REX_SNDTIMEO       
   REX_REUSEADDR
*/

typedef int (*unix_seter) (int fd, void *optval, int optlen);
typedef int (*unix_geter) (int fd, void *optval, int *optlen);

static int unix_get_backlog (int fd, void *optval, int *optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int unix_get_linger (int fd, void *optval, int *optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int unix_get_sndbuf (int fd, void *optval, int *optlen)
{
	int rc;
	int buf = 0;
	socklen_t koptlen = sizeof (buf);

	rc = getsockopt (fd, SOL_SOCKET, SO_SNDBUF, &buf, &koptlen);
	if (rc == 0)
		* (int *) optval = buf;
	return rc;
}

static int unix_get_rcvbuf (int fd, void *optval, int *optlen)
{
	int rc;
	int buf = 0;
	socklen_t koptlen = sizeof (buf);

	rc = getsockopt (fd, SOL_SOCKET, SO_RCVBUF, &buf, &koptlen);
	if (rc == 0)
		* (int *) optval = buf;
	return rc;
}

static int unix_get_noblock (int fd, void *optval, int *optlen)
{
	int flags;

	if ( (flags = fcntl (fd, F_GETFL, 0) ) < 0)
		flags = 0;
	* (int *) optval = (flags & O_NONBLOCK) ? 1 : 0;
	return 0;
}

static int unix_get_nodelay (int fd, void *optval, int *optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int unix_get_sndtimeo (int fd, void *optval, int *optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int unix_get_rcvtimeo (int fd, void *optval, int *optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int unix_get_reuseaddr (int fd, void *optval, int *optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static const unix_geter unix_get_vfptr[] = {
	unix_get_backlog,
	unix_get_linger,
	unix_get_sndbuf,
	unix_get_rcvbuf,
	unix_get_noblock,
	unix_get_nodelay,
	unix_get_rcvtimeo,
	unix_get_sndtimeo,
	unix_get_reuseaddr,
};

static int rex_unix_getopt (struct rex_sock *rs, int opt, void *optval, int *optlen)
{
	int rc;

	if (opt >= NELEM (unix_get_vfptr, unix_geter) || !unix_get_vfptr[opt]) {
		errno = EINVAL;
		return -1;
	}
	rc = unix_get_vfptr[opt] (rs->ss_fd, optval, optlen);
	return rc;
}

static int unix_set_backlog (int fd, void *optval, int optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int unix_set_linger (int fd, void *optval, int optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int unix_set_sndbuf (int fd, void *optval, int optlen)
{
	int rc;
	int buf = * (int *) optval;

	if (buf < 0) {
		errno = EINVAL;
		return -1;
	}
	rc = setsockopt (fd, SOL_SOCKET, SO_SNDBUF, &buf, sizeof (buf) );
	return rc;
}

static int unix_set_rcvbuf (int fd, void *optval, int optlen)
{
	int rc;
	int buf = * (int *) optval;

	if (buf < 0) {
		errno = EINVAL;
		return -1;
	}
	rc = setsockopt (fd, SOL_SOCKET, SO_RCVBUF, &buf, sizeof (buf) );
	return rc;
}

static int unix_set_noblock (int fd, void *optval, int optlen)
{
	int rc;
	int flags;
	int on = * ( (int *) optval);

	if ( (flags = fcntl (fd, F_GETFL, 0) ) < 0)
		flags = 0;
	flags = on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
	rc = fcntl (fd, F_SETFL, flags);
	return rc;
}

static int unix_set_nodelay (int fd, void *optval, int optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int unix_set_sndtimeo (int fd, void *optval, int optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int unix_set_rcvtimeo (int fd, void *optval, int optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int unix_set_reuseaddr (int fd, void *optval, int optlen)
{
	int rc;
	int flags = * (int *) optval ? 1 : 0;

	rc = setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof (flags) );
	return rc;
}

static const unix_seter unix_set_vfptr[] = {
	unix_set_backlog,
	unix_set_linger,
	unix_set_sndbuf,
	unix_set_rcvbuf,
	unix_set_noblock,
	unix_set_nodelay,
	unix_set_rcvtimeo,
	unix_set_sndtimeo,
	unix_set_reuseaddr,
};

static int rex_unix_setopt (struct rex_sock *rs, int opt, void *optval, int optlen)
{
	int rc;

	if (opt >= NELEM (unix_set_vfptr, unix_seter) || !unix_set_vfptr[opt]) {
		errno = EINVAL;
		return -1;
	}
	rc = unix_set_vfptr[opt] (rs->ss_fd, optval, optlen);
	return rc;
}

struct rex_vfptr rex_local = {
	.un_family = REX_AF_LOCAL,
	.init = rex_unix_init,
	.destroy = rex_unix_destroy,
	.listen = rex_unix_listen,
	.accept = rex_unix_accept,
	.connect = rex_unix_connect,
	.send = rex_unix_send,
	.recv = rex_unix_recv,
	.setopt = rex_unix_setopt,
	.getopt = rex_unix_getopt,
};
