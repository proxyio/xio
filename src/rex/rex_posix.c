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

#include "rex.h"
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

enum {
	REX_SODF_BACKLOG = 100,
};

#include "rex_posix_tcp.c"
#include "rex_posix_unix.c"

struct list_head rex_vfptrs;

void __attribute__ ((constructor)) __rex_posix_init (void)
{
	INIT_LIST_HEAD (&rex_vfptrs);
	list_add (&rex_tcp4.item, &rex_vfptrs);
	list_add (&rex_tcp6.item, &rex_vfptrs);
	list_add (&rex_local.item, &rex_vfptrs);
}

void __attribute__ ((destructor)) __rex_posix_exit (void)
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

int rex_sock_send (struct rex_sock *rs, struct rex_iov *iov, int n)
{
	return rs->ss_vfptr->send (rs, iov, n);
}

int rex_sock_recv (struct rex_sock *rs, struct rex_iov *iov, int n)
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
