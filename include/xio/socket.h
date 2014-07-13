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
#define XLISTENER       1
#define XCONNECTOR      2

#define XSOCKADDRLEN 128
int xlisten (const char *sockaddr);
int xaccept (int fd);
int xconnect (const char *sockaddr);

int xrecv (int fd, char **ubuf);
int xsend (int fd, char *ubuf);
int xclose (int fd);

/* Following sockopt-level are provided */
#define XL_SOCKET          1

/* Following sockopt-field are provided */
#define XNOBLOCK           0
#define XSNDWIN            1
#define XRCVWIN            2
#define XSNDBUF            3
#define XRCVBUF            4
#define XLINGER            5
#define XSNDTIMEO          6
#define XRCVTIMEO          7
#define XRECONNECT         8
#define XSOCKTYPE          9
#define XSOCKPROTO         10

int xsetopt (int fd, int level, int opt, void *optval, int optlen);
int xgetopt (int fd, int level, int opt, void *optval, int *optlen);

#include <xio/cplusplus_endif.h>
#endif
