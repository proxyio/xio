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


struct xpoll_entry;
struct xpoll_notify {
    void (*event) (struct xpoll_notify *un, struct xpoll_entry *ent, u32 ev);
};

struct xpoll_entry {
    /* List item for linked other xpoll_entry of the same sock */
    struct list_head xlink;

    /* List item for linked other xpoll_entry of the same poll_table */
    struct list_head lru_link;

    spin_t lock;

    /* Reference hold by x/xpoll_t */
    int ref;

    /* Reference backtrace for debuging */
    int i_idx;
    int incr_tid[10];
    int d_idx;
    int desc_tid[10];
    
    /* struct xpoll_event contain the care events and what happened */
    struct xpoll_event event;
    struct xpoll_notify *notify;
};

#define xpoll_walk_ent(pos, nx, head)					\
    list_for_each_entry_safe(pos, nx, head, struct xpoll_entry, lru_link)

#define xsock_walk_ent(pos, nx, head)					\
    list_for_each_entry_safe(pos, nx, head, struct xpoll_entry, xlink)


struct xpoll_entry *xent_new();
int xent_get(struct xpoll_entry *ent);
int xent_put(struct xpoll_entry *ent);

struct xpoll_t {
    struct xpoll_notify notify;
    int id;
    mutex_t lock;
    condition_t cond;

    /* Reference hold by xpoll_entry */
    atomic_t ref;
    int uwaiters;
    int size;
    struct list_head lru_head;
};

struct xpoll_t *poll_alloc();
struct xpoll_t *pget(int pollid);
void pput(int pollid);

struct xpoll_entry *xpoll_find(struct xpoll_t *po, int xd);
struct xpoll_entry *xpoll_popent(struct xpoll_t *po);
struct xpoll_entry *xpoll_getent(struct xpoll_t *po, int xd);
struct xpoll_entry *xpoll_putent(struct xpoll_t *po, int xd);

void attach_to_xsock(struct xpoll_entry *ent, int xd);
void __detach_from_xsock(struct xpoll_entry *ent);


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
