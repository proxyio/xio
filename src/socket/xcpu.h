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

#ifndef _XIO_XCPU_
#define _XIO_XCPU_

#include <utils/list.h>
#include <utils/mutex.h>
#include <utils/spinlock.h>
#include <utils/eventloop.h>
#include <utils/efd.h>

struct xtask;
typedef void (*xtask_func) (struct xtask *ts);

struct xtask {
    xtask_func f;
    struct list_head link;
};

#define xtask_walk_s(ts, nt, head)			\
    walk_each_entry_s(ts, nt, head, struct xtask, link)

struct xcpu {
    //spin_t lock; // for release mode

    mutex_t lock; // for debug mode

    /* Backend eventloop for cpu_worker. */
    eloop_t el;

    ev_t efd_et;
    struct efd efd;

    /* Waiting for closed xsock will be attached here */
    struct list_head shutdown_socks;
};

int xcpu_alloc();
int xcpu_choosed(int fd);
void xcpu_free(int cpu_no);
struct xcpu *xcpuget(int cpu_no);



#endif
