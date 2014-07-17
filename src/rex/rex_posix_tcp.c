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

static int tcp_listen (const char *host, const char *serv)
{
	int bfd;
	int n;
	int on = 1;
	struct addrinfo hints, *res, *ressave;

	memset (&hints, 0, sizeof (hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((n = getaddrinfo (host, serv, &hints, &res)) != 0)
		return -1;
	ressave = res;
	do {
		if ((bfd = socket (res->ai_family, res->ai_socktype,
				   res->ai_protocol)) < 0)
			continue;
		setsockopt (bfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on));
		if (bind (bfd, res->ai_addr, res->ai_addrlen) == 0)
			break;
		close (bfd);
	} while ((res = res->ai_next) != NULL);
	freeaddrinfo (ressave);
	return bfd;
}

static int rex_tcp_init (struct rex_sock *rs)
{
	rs->ss_fd = -1;
	return 0;
}

static int rex_tcp_destroy (struct rex_sock *rs)
{
	if (rs->ss_fd > 0) {
		close (rs->ss_fd);
		rs->ss_fd = -1;
	}
	rs->ss_family = 0;
	rs->ss_vfptr = 0;
	return 0;
}

/* listen on the sockaddr */
static int rex_tcp_listen (struct rex_sock *rs, const char *sock)
{
	int fd;
	char *host = NULL, *serv = NULL;

	if (!(serv = strrchr (sock, ':'))
	    || strlen (serv + 1) == 0 || ! (host = strdup (sock))) {
		errno = EINVAL;
		return -1;
	}
	host[serv - sock] = '\0';
	if (!(serv = strdup (serv + 1))) {
		free (host);
		return -1;
	}
	fd = tcp_listen (host, serv);
	free (host);
	free (serv);
	if (fd < 0 || listen (fd, REX_SODF_BACKLOG) < 0) {
		close (fd);
		return -1;
	}
	rs->ss_fd = fd;
	return 0;
}

/* accept one new connection for the initialized new sock */
static int rex_tcp_accept (struct rex_sock *rs, struct rex_sock *new)
{
	struct sockaddr_storage addr = {};
	socklen_t addrlen = sizeof (addr);
	int fd = accept (rs->ss_fd, (struct sockaddr *) &addr, &addrlen);

	if (fd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR
		       || errno == ECONNABORTED) ) {
		errno = EAGAIN;
		return -1;
	}
	new->ss_family = rs->ss_family;
	new->ss_fd = fd;
	new->ss_vfptr = rs->ss_vfptr;
	return 0;
}

static int tcp_connect (const char *host, const char *serv)
{
	int fd;
	int n;
	struct addrinfo hints, *res, *ressave;

	memset (&hints, 0, sizeof (hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((n = getaddrinfo (host, serv, &hints, &res)) != 0)
		return -1;
	ressave = res;
	do {
		fd = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
		if (fd < 0)
			continue;
		if (connect (fd, res->ai_addr, res->ai_addrlen) == 0)
			break;
		close (fd);
		fd = -1;
	} while ((res = res->ai_next) != NULL);
	freeaddrinfo (res);
	return fd;
}

/* connect to remote host */
static int rex_tcp_connect (struct rex_sock *rs, const char *peer)
{
	int fd = 0;
	char *host = 0, *serv = 0;

	if (!(serv = strrchr (peer, ':')) || strlen (serv + 1) == 0
	    || !(host = strdup (peer))) {
		errno = EINVAL;
		return -1;
	}
	host[serv - peer] = '\0';
	if (!(serv = strdup (serv + 1))) {
		free (host);
		return -1;
	}
	fd = tcp_connect (host, serv);
	free (host);
	free (serv);
	if (fd < 0)
		return -1;
	rs->ss_fd = fd;
	return 0;
}

static int rex_tcp_send (struct rex_sock *rs, struct rex_iov *iov, int n)
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

static int rex_tcp_recv (struct rex_sock *rs, struct rex_iov *iov, int n)
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

typedef int (*tcp_seter) (int fd, void *optval, int optlen);
typedef int (*tcp_geter) (int fd, void *optval, int *optlen);

static int tcp_get_backlog (int fd, void *optval, int *optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int tcp_get_linger (int fd, void *optval, int *optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int tcp_get_sndbuf (int fd, void *optval, int *optlen)
{
	int rc;
	int buf = 0;
	socklen_t koptlen = sizeof (buf);

	rc = getsockopt (fd, SOL_SOCKET, SO_SNDBUF, &buf, &koptlen);
	if (rc == 0)
		* (int *) optval = buf;
	return rc;
}

static int tcp_get_rcvbuf (int fd, void *optval, int *optlen)
{
	int rc;
	int buf = 0;
	socklen_t koptlen = sizeof (buf);

	rc = getsockopt (fd, SOL_SOCKET, SO_RCVBUF, &buf, &koptlen);
	if (rc == 0)
		* (int *) optval = buf;
	return rc;
}

static int tcp_get_noblock (int fd, void *optval, int *optlen)
{
	int flags;

	if ( (flags = fcntl (fd, F_GETFL, 0) ) < 0)
		flags = 0;
	* (int *) optval = (flags & O_NONBLOCK) ? 1 : 0;
	return 0;
}

static int tcp_get_nodelay (int fd, void *optval, int *optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int tcp_get_sndtimeo (int fd, void *optval, int *optlen)
{
	int rc;
	int to = * (int *) optval;
	struct timeval tv = {
		.tv_sec = to / 1000,
		.tv_usec = (to % 1000) * 1000,
	};
	socklen_t koptlen = sizeof (tv);

	rc = getsockopt (fd, SOL_SOCKET, SO_SNDTIMEO, &tv, &koptlen);
	if (rc == 0)
		* (int *) optval = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	return rc;
}

static int tcp_get_rcvtimeo (int fd, void *optval, int *optlen)
{
	int rc;
	int to = * (int *) optval;
	struct timeval tv = {
		.tv_sec = to / 1000,
		.tv_usec = (to % 1000) * 1000,
	};
	socklen_t koptlen = sizeof (tv);

	rc = getsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, &tv, &koptlen);
	if (rc == 0)
		* (int *) optval = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	return rc;
}

static int tcp_get_reuseaddr (int fd, void *optval, int *optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static const tcp_geter tcp_get_vfptr[] = {
	tcp_get_backlog,
	tcp_get_linger,
	tcp_get_sndbuf,
	tcp_get_rcvbuf,
	tcp_get_noblock,
	tcp_get_nodelay,
	tcp_get_rcvtimeo,
	tcp_get_sndtimeo,
	tcp_get_reuseaddr,
};

static int rex_tcp_getopt (struct rex_sock *rs, int opt, void *optval, int *optlen)
{
	int rc;

	if (opt >= NELEM (tcp_get_vfptr, tcp_geter) || !tcp_get_vfptr[opt]) {
		errno = EINVAL;
		return -1;
	}
	rc = tcp_get_vfptr[opt] (rs->ss_fd, optval, optlen);
	return rc;
}

static int tcp_set_backlog (int fd, void *optval, int optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int tcp_set_linger (int fd, void *optval, int optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int tcp_set_sndbuf (int fd, void *optval, int optlen)
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

static int tcp_set_rcvbuf (int fd, void *optval, int optlen)
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

static int tcp_set_noblock (int fd, void *optval, int optlen)
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

static int tcp_set_nodelay (int fd, void *optval, int optlen)
{
	int rc;
	int flags = * (int *) optval ? 1 : 0;
	rc = setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof (flags) );
	return rc;
}

static int tcp_set_sndtimeo (int fd, void *optval, int optlen)
{
	int rc;
	int to = * (int *) optval;
	struct timeval tv = {
		.tv_sec = to / 1000,
		.tv_usec = (to % 1000) * 1000,
	};
	rc = setsockopt (fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof (tv) );
	return rc;
}

static int tcp_set_rcvtimeo (int fd, void *optval, int optlen)
{
	int rc;
	int to = * (int *) optval;
	struct timeval tv = {
		.tv_sec = to / 1000,
		.tv_usec = (to % 1000) * 1000,
	};
	rc = setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof (tv) );
	return rc;
}

static int tcp_set_reuseaddr (int fd, void *optval, int optlen)
{
	int rc;
	int flags = * (int *) optval ? 1 : 0;

	rc = setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof (flags) );
	return rc;
}

static const tcp_seter tcp_set_vfptr[] = {
	tcp_set_backlog,
	tcp_set_linger,
	tcp_set_sndbuf,
	tcp_set_rcvbuf,
	tcp_set_noblock,
	tcp_set_nodelay,
	tcp_set_rcvtimeo,
	tcp_set_sndtimeo,
	tcp_set_reuseaddr,
};

static int rex_tcp_setopt (struct rex_sock *rs, int opt, void *optval, int optlen)
{
	int rc;

	if (opt >= NELEM (tcp_set_vfptr, tcp_seter) || !tcp_set_vfptr[opt]) {
		errno = EINVAL;
		return -1;
	}
	rc = tcp_set_vfptr[opt] (rs->ss_fd, optval, optlen);
	return rc;
}

struct rex_vfptr rex_tcp4 = {
	.un_family = REX_AF_TCP,
	.init = rex_tcp_init,
	.destroy = rex_tcp_destroy,
	.listen = rex_tcp_listen,
	.accept = rex_tcp_accept,
	.connect = rex_tcp_connect,
	.send = rex_tcp_send,
	.recv = rex_tcp_recv,
	.setopt = rex_tcp_setopt,
	.getopt = rex_tcp_getopt,
};


struct rex_vfptr rex_tcp6 = {
	.un_family = REX_AF_TCP6,
	.init = rex_tcp_init,
	.destroy = rex_tcp_destroy,
	.listen = rex_tcp_listen,
	.accept = rex_tcp_accept,
	.connect = rex_tcp_connect,
	.send = rex_tcp_send,
	.recv = rex_tcp_recv,
	.setopt = rex_tcp_setopt,
	.getopt = rex_tcp_getopt,
};

