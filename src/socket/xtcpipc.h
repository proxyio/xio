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

#ifndef _XIO_TCPIPC_SOCK_
#define _XIO_TCPIPC_SOCK_

#include "xsock.h"

struct tcpipc_sock {
    struct sockbase base;
    ev_t et;
    struct bio in;
    struct bio out;
    struct io ops;
    int sys_fd;
    struct iovec iov[100];
    struct iovec *biov;
    int iov_start;
    int iov_end;
    int iov_length;
    struct list_head sg_head;
    struct transport_vf *tp_vfptr;
};

extern struct sockbase_vfptr xtcp_listener_spec;
extern struct sockbase_vfptr xtcp_connector_spec;
extern struct sockbase_vfptr xipc_listener_spec;
extern struct sockbase_vfptr xipc_connector_spec;

#endif
