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

#ifndef _HPIO_TRANSPORT_
#define _HPIO_TRANSPORT_

#include "ds/list.h"

#define TP_TCP            1
#define TP_IPC            2
#define TP_INPROC         4

#define TP_LINGER         1
#define TP_SNDBUF         2
#define TP_RCVBUF         3
#define TP_NOBLOCK        4
#define TP_NODELAY        5
#define TP_RCVTIMEO       6
#define TP_SNDTIMEO       7
#define TP_REUSEADDR      8

typedef int (*tp_setopt) (int fd, void *optval, int optlen);
typedef int (*tp_getopt) (int fd, void *optval, int *optlen);


#define TP_SOCKADDRLEN    128

struct transport {
    const char *name;
    int proto;
    void (*global_init) (void);
    void (*close) (int fd);
    int (*bind) (const char *sock);
    int (*accept) (int fd);
    int (*connect) (const char *peer);
    int64_t (*read) (int fd, char *buff, int64_t size);
    int64_t (*write) (int fd, const char *buff, int64_t size);
    int (*setopt) (int fd, int opt, void *val, int vallen);
    int (*getopt) (int fd, int opt, void *val, int vallen);
    struct list_head item;
};

struct transport *transport_lookup(int proto);


#endif
