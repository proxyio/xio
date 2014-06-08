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

#ifndef _XIO_XPOLL_
#define _XIO_XPOLL_

#include <utils/atomic.h>
#include <utils/mutex.h>
#include <utils/spinlock.h>
#include <utils/condition.h>
#include <utils/list.h>
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

#define walk_xpitem_s(itm, nitm, head)				\
    walk_each_entry_s(itm, nitm, head, struct xpitem, lru_link)

struct xpitem *xpitem_alloc();
int xpitem_get (struct xpitem *itm);
int xpitem_put (struct xpitem *itm);


struct xpoll_t {
	mutex_t lock;
	condition_t cond;
	u64 shutdown_state:1;
	int id;
	atomic_t ref;
	int uwaiters;
	int size;
	struct list_head lru_head;
};

struct xpoll_t *poll_alloc();
struct xpoll_t *pget (int pollid);
void pput (int pollid);

#define XPOLL_HEADFD -0x3654
struct xpitem *getfd (struct xpoll_t *poll, int fd);
struct xpitem *addfd (struct xpoll_t *poll, int fd);
int rmfd (struct xpoll_t *poll, int fd);

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
