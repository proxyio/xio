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

#ifndef _H_PROXYIO_SOCKET_
#define _H_PROXYIO_SOCKET_

#include <inttypes.h>
#include <xio/cplusplus_define.h>
#include <xio/cmsghdr.h>

/* Following socktype are provided */
enum {
	XLISTENER       =    1,
	XCONNECTOR,
	XSOCKADDRLEN    =   128,
};

int xlisten (const char *sockaddr);
int xaccept (int fd);
int xconnect (const char *sockaddr);

int xrecv (int fd, char **ubuf);
int xsend (int fd, char *ubuf);
int xclose (int fd);

/* Following sockopt-field are provided */
enum {
	XSO_LINGER       =     0,
	XSO_SNDBUF,
	XSO_RCVBUF,
	XSO_NOBLOCK,
	XSO_NODELAY,
	XSO_SNDTIMEO,
	XSO_RCVTIMEO,
	XSO_SOCKTYPE,
	XSO_SOCKPROTO,
	XSO_VERBOSE,
};

int xsetopt (int fd, int opt, void *optval, int optlen);
int xgetopt (int fd, int opt, void *optval, int *optlen);

#include <xio/cplusplus_endif.h>
#endif
