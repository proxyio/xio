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

#ifndef _H_PROXYIO_POLL_STRUCT_
#define _H_PROXYIO_POLL_STRUCT_

#include <utils/atomic.h>
#include <utils/mutex.h>
#include <utils/spinlock.h>
#include <utils/condition.h>
#include <utils/list.h>
#include <xio/poll.h>
#include <socket/sockbase.h>
#include "stats.h"

struct poll_entry {
	struct pollbase base;
	spin_t lock;
	struct list_head lru_link;
	int ref;
	struct xpoll_t *owner;
};

extern struct pollbase_vfptr xpollbase_vfptr;

#define walk_poll_entry_s(pfd, tmp, head)				\
    walk_each_entry_s(pfd, tmp, head, struct poll_entry, lru_link)

struct poll_entry *poll_entry_alloc();
int poll_entry_get (struct poll_entry *pfd);
int poll_entry_put (struct poll_entry *pfd);


struct xpoll_t {
	mutex_t lock;
	condition_t cond;
	u64 shutdown_state:1;
	int id;
	atomic_t ref;
	int uwaiters;
	int size;
	struct list_head lru_head;
	struct poll_mstats stats;
};

struct xpoll_t *poll_alloc();
struct xpoll_t *poll_get (int pollid);
void poll_put (int pollid);

#define XPOLL_HEADFD -0x3654
struct poll_entry *get_poll_entry (struct xpoll_t *poll, int fd);
struct poll_entry *add_poll_entry (struct xpoll_t *poll, int fd);
int rm_poll_entry (struct xpoll_t *poll, int fd);


/* Max number of concurrent socks. */
#define PROXYIO_MAX_POLLS 10240

struct pglobal {
	spin_t lock;

	/* The global table of existing xsock. The descriptor representing
	 * the xsock is the index to this table. This pointer is also used to
	 * find out whether context is initialised. If it is null, context is
	 * uninitialised.
	 */
	struct xpoll_t *polls[PROXYIO_MAX_POLLS];

	/* Stack of unused xsock descriptors.  */
	int unused[PROXYIO_MAX_POLLS];

	/* Number of actual socks. */
	size_t npolls;
};

extern struct pglobal pg;



#endif
