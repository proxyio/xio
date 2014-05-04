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

#ifndef _HPIO_XGLOBAL_
#define _HPIO_XGLOBAL_

#include <sync/mutex.h>
#include <ds/list.h>
#include "xsock.h"
#include "xcpu.h"


/* Max number of cpu core */
#define XIO_MAX_CPUS 32

struct xglobal {
    int exiting;
    mutex_t lock;

    /* The global table of existing xsock. The descriptor representing
     * the xsock is the index to this table. This pointer is also used to
     * find out whether context is initialised. If it is null, context is
     * uninitialised.
     */
    struct xsock socks[XIO_MAX_SOCKS];

    /* Stack of unused xsock descriptors.  */
    int unused[XIO_MAX_SOCKS];

    /* Number of actual socks. */
    size_t nsocks;
    

    struct xcpu cpus[XIO_MAX_CPUS];

    /* Stack of unused xsock descriptors.  */
    int cpu_unused[XIO_MAX_CPUS];
    
    /* Number of actual runner poller.  */
    size_t ncpus;
    size_t ncpus_low;
    size_t ncpus_high;

    /* Backend cpu_cores and taskpool for cpu_worker.  */
    int cpu_cores;
    struct taskpool tpool;

    /* INPROC global listening address mapping */
    struct ssmap inproc_listeners;

    /* pfspec head */
    struct list_head pfspec_head;
};

#define pfspec_walk_safe(pos, nx, head)			\
    list_for_each_entry_safe(pos, nx, head,		\
			     struct pfspec, link)

struct pfspec *proto_lookup(int pf, int type);

extern struct xglobal xgb;

static inline void xglobal_lock() {
    mutex_lock(&xgb.lock);
}

static inline void xglobal_unlock() {
    mutex_unlock(&xgb.lock);
}



#endif
