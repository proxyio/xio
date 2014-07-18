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

/* the following af_family are support by rex:
   AF_LOCAL: the localhost communication
   AF_TCP:   the tcp4 communication
   AF_TCP6:  the tcp6 communication */
int rex_sock_init (struct rex_sock *rs, int family /* AF_LOCAL|AF_TCP|AF_TCP6 */)
{
}

int rex_sock_destroy (struct rex_sock *rs)
{
}

/* listen on the sockaddr with a backlog hint for kernel */
int rex_sock_listen (struct rex_sock *rs, const char *sockaddr, int backlog)
{
}

/* accept one new connection for the initialized new sock */
int rex_sock_accept (struct rex_sock *rs, struct rex_sock *new)
{
}

/* connect to remote host */
int rex_sock_connect (struct rex_sock *rs, const char *sockaddr)
{
}

int rex_sock_sendv (struct rex_sock *rs, struct rex_iov *iov, int niov)
{
}

int rex_sock_recvv (struct rex_sock *rs, struct rex_iov *iov, int niov)
{
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
}

int rex_sock_getopt (struct rex_sock *rs, int opt, void *optval, int *optlen)
{
}
