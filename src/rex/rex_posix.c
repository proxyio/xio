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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "rex.h"
#include "rex_if.h"

enum {
	REX_MAX_BACKLOG = 100,
};

static int rex_gen_init (struct rex_sock *rs)
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
static int rex_tcp_listen (struct rex_sock *rs, const char *sock)
{
	int fd;
	int n;
	int on = 1;
	char *host = NULL, *serv = NULL;
	struct addrinfo hints, *res, *ressave;
	
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

	memset (&hints, 0, sizeof (hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((n = getaddrinfo (host, serv, &hints, &res)) != 0)
		return -1;
	ressave = res;
	do {
		if ((fd = socket (res->ai_family, res->ai_socktype,
				   res->ai_protocol)) < 0)
			continue;
		setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on));
		if (bind (fd, res->ai_addr, res->ai_addrlen) == 0)
			break;
		close (fd);
	} while ((res = res->ai_next) != NULL);
	freeaddrinfo (ressave);

	free (host);
	free (serv);
	if (fd < 0 || listen (fd, REX_MAX_BACKLOG) < 0) {
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
		       || errno == ECONNABORTED)) {
		errno = EAGAIN;
		return -1;
	}
	new->ss_family = rs->ss_family;
	new->ss_fd = fd;
	new->ss_vfptr = rs->ss_vfptr;
	return 0;
}

static int rex_tcp_connect (struct rex_sock *rs, const char *peer)
{
	int fd = 0;
	int n;
	char *host = 0, *serv = 0;
	struct addrinfo hints, *res, *ressave;

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

	free (host);
	free (serv);
	if (fd < 0)
		return -1;
	rs->ss_fd = fd;
	return 0;
}

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
	    || listen (fd, REX_MAX_BACKLOG) < 0) {
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
	     errno == ECONNABORTED || errno == EMFILE)) {
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

static int rex_gen_send (struct rex_sock *rs, struct rex_iov *iov, int n)
{
	struct iovec diov[100];    /* local storage for performance */
	struct msghdr msg = {};
	int i;
	int rc;
	
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
	rc = sendmsg (rs->ss_fd, &msg, 0);

	/* Several errors are OK. When speculative write is being done we
	   may not be able to write a single byte to the socket. Also, SIGSTOP
	   issued by a debugging tool can result in EINTR error. */
	if (rc == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
			errno = EAGAIN;
			return -1;
		}
		/* Signalise peer failure. */
		errno = EPIPE;
		return -1;
	}
	return rc;
}

static int rex_gen_recv (struct rex_sock *rs, struct rex_iov *iov, int n)
{
	struct iovec diov[100];
	struct msghdr msg = {};
	int i;
	int rc;

	if (n > 100)
		n = 100;
	for (i = 0; i < n; i++) {
		diov[i].iov_base = iov[i].iov_base;
		diov[i].iov_len = iov[i].iov_len;
	}
	msg.msg_iov = diov;
	msg.msg_iovlen = n;
	rc = recvmsg (rs->ss_fd, &msg, 0);

	/* Signalise peer failure. */
	if (rc == 0) {
		errno = EPIPE;
		return -1;
	}
	/* Several errors are OK. When speculative read is being done we
	   may not be able to read a single byte to the socket. Also, SIGSTOP
	   issued by a debugging tool can result in EINTR error. */
	if (rc == -1 &&
	    (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
		errno = EAGAIN;
		return -1;
	}
	return rc;
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

typedef int (*rex_gen_seter) (struct rex_sock *rs, void *optval, int optlen);
typedef int (*rex_gen_geter) (struct rex_sock *rs, void *optval, int *optlen);

static int get_backlog (struct rex_sock *rs, void *optval, int *optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int get_linger (struct rex_sock *rs, void *optval, int *optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int get_sndbuf (struct rex_sock *rs, void *optval, int *optlen)
{
	int rc;
	int buf = 0;
	socklen_t koptlen = sizeof (buf);

	rc = getsockopt (rs->ss_fd, SOL_SOCKET, SO_SNDBUF, &buf, &koptlen);
	if (rc == 0)
		* (int *) optval = buf;
	return rc;
}

static int get_rcvbuf (struct rex_sock *rs, void *optval, int *optlen)
{
	int rc;
	int buf = 0;
	socklen_t koptlen = sizeof (buf);

	rc = getsockopt (rs->ss_fd, SOL_SOCKET, SO_RCVBUF, &buf, &koptlen);
	if (rc == 0)
		* (int *) optval = buf;
	return rc;
}

static int get_noblock (struct rex_sock *rs, void *optval, int *optlen)
{
	int flags;

	if ((flags = fcntl (rs->ss_fd, F_GETFL, 0)) < 0)
		flags = 0;
	* (int *) optval = (flags & O_NONBLOCK) ? 1 : 0;
	return 0;
}

static int get_nodelay (struct rex_sock *rs, void *optval, int *optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int get_sndtimeo (struct rex_sock *rs, void *optval, int *optlen)
{
	int rc;
	int to = * (int *) optval;
	struct timeval tv = {
		.tv_sec = to / 1000,
		.tv_usec = (to % 1000) * 1000,
	};
	socklen_t koptlen = sizeof (tv);

	rc = getsockopt (rs->ss_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, &koptlen);
	if (rc == 0)
		* (int *) optval = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	return rc;
}

static int get_rcvtimeo (struct rex_sock *rs, void *optval, int *optlen)
{
	int rc;
	int to = * (int *) optval;
	struct timeval tv = {
		.tv_sec = to / 1000,
		.tv_usec = (to % 1000) * 1000,
	};
	socklen_t koptlen = sizeof (tv);

	rc = getsockopt (rs->ss_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, &koptlen);
	if (rc == 0)
		* (int *) optval = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	return rc;
}

static int get_reuseaddr (struct rex_sock *rs, void *optval, int *optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int get_sockname (struct rex_sock *rs, void *optval, int *optlen)
{
	int leng = *optlen;

	if (leng > strlen (rs->ss_addr))
		leng = strlen (rs->ss_addr);
	memcpy (optval, rs->ss_addr, leng);
	*optlen = leng;
	return 0;
}

static int get_peername (struct rex_sock *rs, void *optval, int *optlen)
{
	int leng = *optlen;

	if (leng > strlen (rs->ss_peer))
		leng = strlen (rs->ss_peer);
	memcpy (optval, rs->ss_peer, leng);
	*optlen = leng;
	return 0;
}

static const rex_gen_geter get_vfptr[] = {
	get_backlog,
	get_linger,
	get_sndbuf,
	get_rcvbuf,
	get_noblock,
	get_nodelay,
	get_rcvtimeo,
	get_sndtimeo,
	get_reuseaddr,
	get_sockname,
	get_peername,
};

static int rex_gen_getopt (struct rex_sock *rs, int opt, void *optval, int *optlen)
{
	int rc;

	if (opt >= NELEM (get_vfptr, rex_gen_geter) || !get_vfptr[opt]) {
		errno = EINVAL;
		return -1;
	}
	rc = get_vfptr[opt] (rs, optval, optlen);
	return rc;
}

static int set_backlog (struct rex_sock *rs, void *optval, int optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int set_linger (struct rex_sock *rs, void *optval, int optlen)
{
	errno = EOPNOTSUPP;
	return -1;
}

static int set_sndbuf (struct rex_sock *rs, void *optval, int optlen)
{
	int rc;
	int buf = * (int *) optval;

	if (buf < 0) {
		errno = EINVAL;
		return -1;
	}
	rc = setsockopt (rs->ss_fd, SOL_SOCKET, SO_SNDBUF, &buf, sizeof (buf));
	return rc;
}

static int set_rcvbuf (struct rex_sock *rs, void *optval, int optlen)
{
	int rc;
	int buf = * (int *) optval;

	if (buf < 0) {
		errno = EINVAL;
		return -1;
	}
	rc = setsockopt (rs->ss_fd, SOL_SOCKET, SO_RCVBUF, &buf, sizeof (buf));
	return rc;
}

static int set_noblock (struct rex_sock *rs, void *optval, int optlen)
{
	int rc;
	int flags;
	int on = * ((int *) optval);

	if ((flags = fcntl (rs->ss_fd, F_GETFL, 0)) < 0)
		flags = 0;
	flags = on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
	rc = fcntl (rs->ss_fd, F_SETFL, flags);
	return rc;
}

static int set_nodelay (struct rex_sock *rs, void *optval, int optlen)
{
	int rc;
	int flags = * (int *) optval ? 1 : 0;
	rc = setsockopt (rs->ss_fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof (flags));
	return rc;
}

static int set_sndtimeo (struct rex_sock *rs, void *optval, int optlen)
{
	int rc;
	int to = * (int *) optval;
	struct timeval tv = {
		.tv_sec = to / 1000,
		.tv_usec = (to % 1000) * 1000,
	};
	rc = setsockopt (rs->ss_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof (tv));
	return rc;
}

static int set_rcvtimeo (struct rex_sock *rs, void *optval, int optlen)
{
	int rc;
	int to = * (int *) optval;
	struct timeval tv = {
		.tv_sec = to / 1000,
		.tv_usec = (to % 1000) * 1000,
	};
	rc = setsockopt (rs->ss_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof (tv));
	return rc;
}

static int set_reuseaddr (struct rex_sock *rs, void *optval, int optlen)
{
	int rc;
	int flags = * (int *) optval ? 1 : 0;

	rc = setsockopt (rs->ss_fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof (flags));
	return rc;
}

static const rex_gen_seter set_vfptr[] = {
	set_backlog,
	set_linger,
	set_sndbuf,
	set_rcvbuf,
	set_noblock,
	set_nodelay,
	set_rcvtimeo,
	set_sndtimeo,
	set_reuseaddr,
	0,
	0,
};

static int rex_gen_setopt (struct rex_sock *rs, int opt, void *optval, int optlen)
{
	int rc;

	if (opt >= NELEM (set_vfptr, rex_gen_seter) || !set_vfptr[opt]) {
		errno = EINVAL;
		return -1;
	}
	rc = set_vfptr[opt] (rs, optval, optlen);
	return rc;
}

struct rex_vfptr rex_tcp4 = {
	.un_family = REX_AF_TCP,
	.init = rex_gen_init,
	.destroy = rex_tcp_destroy,
	.listen = rex_tcp_listen,
	.accept = rex_tcp_accept,
	.connect = rex_tcp_connect,
	.send = rex_gen_send,
	.recv = rex_gen_recv,
	.setopt = rex_gen_setopt,
	.getopt = rex_gen_getopt,
};


struct rex_vfptr rex_tcp6 = {
	.un_family = REX_AF_TCP6,
	.init = rex_gen_init,
	.destroy = rex_tcp_destroy,
	.listen = rex_tcp_listen,
	.accept = rex_tcp_accept,
	.connect = rex_tcp_connect,
	.send = rex_gen_send,
	.recv = rex_gen_recv,
	.setopt = rex_gen_setopt,
	.getopt = rex_gen_getopt,
};

struct rex_vfptr rex_local = {
	.un_family = REX_AF_LOCAL,
	.init = rex_gen_init,
	.destroy = rex_unix_destroy,
	.listen = rex_unix_listen,
	.accept = rex_unix_accept,
	.connect = rex_unix_connect,
	.send = rex_gen_send,
	.recv = rex_gen_recv,
	.setopt = rex_gen_setopt,
	.getopt = rex_gen_getopt,
};

struct list_head rex_vfptrs;

void __rex_init (void)
{
	INIT_LIST_HEAD (&rex_vfptrs);
	list_add (&rex_tcp4.item, &rex_vfptrs);
	list_add (&rex_tcp6.item, &rex_vfptrs);
	list_add (&rex_local.item, &rex_vfptrs);
}

void __rex_exit (void)
{
}

struct rex_vfptr *get_rex_vfptr (int un_family)
{
	struct rex_vfptr *pos;
	struct rex_vfptr *tmp;

	walk_each_entry_s (pos, tmp, &rex_vfptrs, struct rex_vfptr, item) {
		if (pos->un_family == un_family)
			return pos;
	}
	return 0;
}



/* the following af_family are support by rex:
   AF_LOCAL: the localhost communication
   AF_TCP:   the tcp4 communication
   AF_TCP6:  the tcp6 communication */
int rex_sock_init (struct rex_sock *rs, int family /* AF_LOCAL|AF_TCP|AF_TCP6 */)
{
	rs->ss_family = family;
	rs->ss_addr = rs->ss_peer = 0;
	if (!(rs->ss_vfptr = get_rex_vfptr (family))) {
		errno = EPROTO;
		return -1;
	}
	return rs->ss_vfptr->init (rs);
}

int rex_sock_destroy (struct rex_sock *rs)
{
	int rc;
	if ((rc = rs->ss_vfptr->destroy (rs)) == 0) {
		if (rs->ss_addr) {
			free (rs->ss_addr);
			rs->ss_addr = 0;
		}
		if (rs->ss_peer) {
			free (rs->ss_peer);
			rs->ss_peer = 0;
		}
	}
	return rc;
}

/* listen on the sockaddr */
int rex_sock_listen (struct rex_sock *rs, const char *sock)
{
	int rc;
	if ((rc = rs->ss_vfptr->listen (rs, sock)) == 0)
		rs->ss_addr = strdup (sock);
	return rc;
}

/* accept one new connection for the initialized new sock */
int rex_sock_accept (struct rex_sock *rs, struct rex_sock *new)
{
	if (new->ss_family != rs->ss_family || new->ss_vfptr != rs->ss_vfptr) {
		errno = EINVAL;
		return -1;
	}
	return rs->ss_vfptr->accept (rs, new);
}

/* connect to remote host */
int rex_sock_connect (struct rex_sock *rs, const char *peer)
{
	int rc;
	if ((rc = rs->ss_vfptr->connect (rs, peer)) == 0)
		rs->ss_peer = strdup (peer);
	return rc;
}

int rex_sock_sendv (struct rex_sock *rs, struct rex_iov *iov, int n)
{
	return rs->ss_vfptr->send (rs, iov, n);
}

int rex_sock_recvv (struct rex_sock *rs, struct rex_iov *iov, int n)
{
	return rs->ss_vfptr->recv (rs, iov, n);
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
int rex_sock_setopt (struct rex_sock *rs, int opt, void *optval, int optlen)
{
	return rs->ss_vfptr->setopt (rs, opt, optval, optlen);
}

int rex_sock_getopt (struct rex_sock *rs, int opt, void *optval, int *optlen)
{
	return rs->ss_vfptr->getopt (rs, opt, optval, optlen);
}
