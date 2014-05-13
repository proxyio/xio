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

#ifndef _HPIO_TCP_
#define _HPIO_TCP_

#include <inttypes.h>
#include "transport/transport.h"

void tcp_close(int fd);
int tcp_bind(const char *sock);
int tcp_accept(int fd);
int tcp_connect(const char *peer);

int tcp_setopt(int fd, int opt, void *optval, int optlen);
int tcp_getopt(int fd, int opt, void *optval, int *optlen);

int64_t tcp_read(int fd, char *buff, int64_t size);
int64_t tcp_write(int fd, const char *buff, int64_t size);
int tcp_sockname(int fd, char *sockname, int size);
int tcp_peername(int fd, char *peername, int size);

#endif
