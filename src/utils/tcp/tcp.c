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

#include <config.h>
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

#define TP_TCP_BACKLOG 100

void tcp_close (int fd)
{
	close (fd);
}

int __unp_bind (const char *host, const char *serv)
{
	int listenfd, n;
	const int on = 1;
	struct addrinfo hints, *res, *ressave;

	ZERO (hints);
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ( (n = getaddrinfo (host, serv, &hints, &res) ) != 0)
		return -1;
	ressave = res;
	do {
		if ( (listenfd = socket (res->ai_family, res->ai_socktype,
		                         res->ai_protocol) ) < 0)
			continue;
		setsockopt (listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on) );
		if (bind (listenfd, res->ai_addr, res->ai_addrlen) == 0)
			break;
		close (listenfd);
	} while ( (res = res->ai_next) != NULL);
	freeaddrinfo (ressave);
	return listenfd;
}

int tcp_bind (const char *sock)
{
	int afd;
	char *host = NULL, *serv = NULL;

	if (! (serv = strrchr (sock, ':') )
	    || strlen (serv + 1) == 0 || ! (host = strdup (sock) ) )
		return -1;
	host[serv - sock] = '\0';
	if (! (serv = strdup (serv + 1) ) ) {
		free (host);
		return -1;
	}
	afd = __unp_bind (host, serv);
	free (host);
	free (serv);
	if (afd < 0 || listen (afd, TP_TCP_BACKLOG) < 0) {
		close (afd);
		return -1;
	}
	return afd;
}

int tcp_accept (int afd)
{
	struct sockaddr_storage addr = {};
	socklen_t addrlen = sizeof (addr);
	int fd = accept (afd, (struct sockaddr *) &addr, &addrlen);

	if (fd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK
	               || errno == EINTR || errno == ECONNABORTED) ) {
		errno = EAGAIN;
		return -1;
	}
	return fd;
}

int __unp_connect (const char *host, const char *serv)
{
	int sockfd, n;
	struct addrinfo hints, *res, *ressave;

	ZERO (hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ( (n = getaddrinfo (host, serv, &hints, &res) ) != 0)
		return -1;
	ressave = res;
	do {
		sockfd = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sockfd < 0)
			continue;
		if (connect (sockfd, res->ai_addr, res->ai_addrlen) == 0)
			break;
		close (sockfd);
		sockfd = -1;
	} while ( (res = res->ai_next) != NULL);
	freeaddrinfo (res);
	return sockfd;
}

int tcp_connect (const char *peer)
{
	int fd = 0;
	char *host = NULL, *serv = NULL;

	if (! (serv = strrchr (peer, ':') )
	    || strlen (serv + 1) == 0 || ! (host = strdup (peer) ) )
		return -1;
	host[serv - peer] = '\0';
	if (! (serv = strdup (serv + 1) ) ) {
		free (host);
		return -1;
	}
	fd = __unp_connect (host, serv);
	free (host);
	free (serv);
	return fd;
}


i64 tcp_recv (int sockfd, char *buf, i64 len)
{
	i64 rc;
	rc = recv (sockfd, buf, len, 0);

	/* Signalise peer failure. */
	if (rc == 0) {
		errno = EPIPE;
		return -1;
	}
	/* Several errors are OK. When speculative read is being done we
	 * may not be able to read a single byte to the socket. Also, SIGSTOP
	 * issued by a debugging tool can result in EINTR error. */
	if (rc == -1 &&
	    (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) ) {
		errno = EAGAIN;
		return -1;
	}
	return rc;
}

i64 tcp_send (int sockfd, const char *buf, i64 len)
{
#if defined HAVE_DEBUG
	i64 rc = send (sockfd, buf, (rand () % len) + 1, 0);
#else
	i64 rc = send (sockfd, buf, len, 0);
#endif

	/* Several errors are OK. When speculative write is being done we
	 * may not be able to write a single byte to the socket. Also, SIGSTOP
	 * issued by a debugging tool can result in EINTR error. */
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

i64 tcp_sendmsg (int sockfd, const struct msghdr *msg, int flags)
{
	i64 rc = sendmsg (sockfd, msg, flags);

	/* Several errors are OK. When speculative write is being done we
	 * may not be able to write a single byte to the socket. Also, SIGSTOP
	 * issued by a debugging tool can result in EINTR error. */
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

int tcp_sockname (int fd, char *sock, int size)
{
	struct sockaddr_storage addr = {};
	socklen_t addrlen = sizeof (addr);
	struct sockaddr_in *sa_in;
	char tcp_addr[TP_SOCKADDRLEN] = {};

	if (-1 == getsockname (fd, (struct sockaddr *) &addr, &addrlen) )
		return -1;
	sa_in = (struct sockaddr_in *) &addr;
	inet_ntop (AF_INET, (char *) &sa_in->sin_addr, tcp_addr, sizeof (tcp_addr) );
	snprintf (tcp_addr + strlen (tcp_addr),
	          sizeof (tcp_addr) - strlen (tcp_addr), ":%d", ntohs (sa_in->sin_port) );
	size = size > TP_SOCKADDRLEN - 1 ? TP_SOCKADDRLEN - 1 : size;
	sock[size] = '\0';
	memcpy (sock, tcp_addr, size);
	return 0;
}

int tcp_peername (int fd, char *peer, int size)
{
	struct sockaddr_storage addr = {};
	socklen_t addrlen = sizeof (addr);
	struct sockaddr_in *sa_in;
	char tcp_addr[TP_SOCKADDRLEN] = {};

	if (-1 == getpeername (fd, (struct sockaddr *) &addr, &addrlen) )
		return -1;
	sa_in = (struct sockaddr_in *) &addr;
	inet_ntop (AF_INET, (char *) &sa_in->sin_addr, tcp_addr, sizeof (tcp_addr) );
	snprintf (tcp_addr + strlen (tcp_addr),
	          sizeof (tcp_addr) - strlen (tcp_addr), ":%d", ntohs (sa_in->sin_port) );
	size = size > TP_SOCKADDRLEN - 1 ? TP_SOCKADDRLEN - 1 : size;
	peer[size] = '\0';
	memcpy (peer, tcp_addr, size);
	return 0;
}



static struct transport tcp_ops = {
	.name = "tcp",
	.proto = TP_TCP,
	.close = tcp_close,
	.bind = tcp_bind,
	.accept = tcp_accept,
	.connect = tcp_connect,
	.recv = tcp_recv,
	.send = tcp_send,
	.sendmsg = tcp_sendmsg,
	.setopt = tcp_setopt,
	.getopt = tcp_getopt,
	.item = LIST_ITEM_INITIALIZE,
};

struct transport *tcp_vfptr = &tcp_ops;
