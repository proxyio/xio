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

#ifndef _HPIO_XPOLL_
#define _HPIO_XPOLL_

#include <sync/atomic.h>
#include <sync/mutex.h>
#include <sync/spin.h>
#include <sync/condition.h>
#include <ds/list.h>
#include <xio/poll.h>
#include <socket/xsock.h>

struct xpitem {
    struct pollbase base;
    spin_t lock;
    struct list_head lru_link;
    int ref;
    struct xpoll_t *poll;
};

extern struct pollbase_vfptr xpollbase_vfptr;

#define xpoll_walk_ent(pos, nx, head)				\
    list_for_each_entry_safe(pos, nx, head,			\
			     struct xpitem, lru_link)

struct xpitem *xent_new();
int xent_get(struct xpitem *ent);
int xent_put(struct xpitem *ent);

struct xpoll_t {
    int id;
    mutex_t lock;
    condition_t cond;
    atomic_t ref;
    int uwaiters;
    int size;
    struct list_head lru_head;
};

struct xpoll_t *poll_alloc();
struct xpoll_t *pget(int pollid);
void pput(int pollid);

struct xpitem *xpoll_find(struct xpoll_t *po, int xd);
struct xpitem *xpoll_popent(struct xpoll_t *po);
struct xpitem *xpoll_getent(struct xpoll_t *po, int xd);
struct xpitem *xpoll_putent(struct xpoll_t *po, int xd);

/* Max number of concurrent socks. */
#define XIO_MAX_POLLS 10240

struct xp_global {
    spin_t lock;

    /* The global table of existing xsock. The descriptor representing
     * the xsock is the index to this table. This pointer is also used to
     * find out whether context is initialised. If it is null, context is
     * uninitialised.
     */
    struct xpoll_t *polls[XIO_MAX_POLLS];

    /* Stack of unused xsock descriptors.  */
    int unused[XIO_MAX_POLLS];

    /* Number of actual socks. */
    size_t npolls;
};

extern struct xp_global pg;



#endif
