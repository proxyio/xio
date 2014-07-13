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

#ifndef _H_PROXYIO_SP_MODULE_
#define _H_PROXYIO_SP_MODULE_

#include <uuid/uuid.h>
#include <utils/list.h>
#include <utils/mutex.h>
#include <utils/atomic.h>
#include <utils/spinlock.h>
#include <utils/condition.h>
#include <utils/thread.h>
#include <utils/mstats_base.h>
#include <xio/poll.h>
#include <xio/sp_reqrep.h>
#include <msgbuf/msgbuf_head.h>
#include "sp_hdr.h"

enum {
	EP_SEND = 0,
	EP_RECV,
	EPBASE_STATS_KEYRANGE,
};

DEFINE_MSTATS (epbase, EPBASE_STATS_KEYRANGE);

enum {
	TG_SEND = 0,
	TG_RECV,
	TGTD_STATS_KEYRANGE,
};

DEFINE_MSTATS (tgtd, TGTD_STATS_KEYRANGE);


static inline int get_socktype (int fd)
{
	int socktype = 0, rc;
	int optlen = sizeof (socktype);

	BUG_ON ( (rc = xgetopt (fd, XL_SOCKET, XSOCKTYPE, &socktype, &optlen) ) );
	return socktype;
}

struct epbase;
struct tgtd;
struct epbase_vfptr {
	int sp_family;
	int sp_type;

	/* Alloc scalability protocol's specified endpoint and return the epbase pointer */
	struct epbase * (*alloc) ();

	/* Destroy endpoint, sp_module doesn't use it anymore */
	void            (*destroy) (struct epbase *ep);

	/* Call by sp_send() API, have message need send */
	int             (*send)    (struct epbase *ep, char *ubuf);

	/* The current target sock specified by tg parameter is actived. take one message
	 * from the epbase->snd.head and send into network */
	int             (*rm)      (struct epbase *ep, struct tgtd *tg,
				    char **ubuf);

	/* The incoming message specified by ubuf pointer */
	int             (*add)     (struct epbase *ep, struct tgtd *tg,
				    char *ubuf);

	struct tgtd *   (*join)    (struct epbase *ep, int fd);
	void            (*term)    (struct epbase *ep, struct tgtd *tg);
	int             (*setopt)  (struct epbase *ep, int opt, void *optval,
				    int optlen);
	int             (*getopt)  (struct epbase *ep, int opt, void *optval,
				    int *optlen);
	struct list_head item;
};



struct tgtd {
	struct epbase *owner;           /* the owner of this target socket */
	struct poll_fd pollfd;          /* xpoll entry */
	u32 bad_status:1;
	int fd;                         /* xsocket file descriptor */
	struct list_head item;
	struct tgtd_mstats stats;
};

void generic_tgtd_init (struct epbase *ep, struct tgtd *tg, int fd);

void sg_add_tg (struct tgtd *tg);
void sg_rm_tg (struct tgtd *tg);
void sg_update_tg (struct tgtd *tg, u32 ev);

int sp_generic_term_by_tgtd (struct epbase *ep, struct tgtd *tg);
int sp_generic_term_by_fd (struct epbase *ep, int fd);

static inline void tgtd_try_enable_out (struct tgtd *tg)
{
	struct epbase *ep = tg->owner;

	if (! (tg->pollfd.events & XPOLLOUT) ) {
		sg_update_tg (tg, tg->pollfd.events | XPOLLOUT);
		DEBUG_OFF ("ep %d socket %d enable pollout", ep->eid, tg->fd);
	}
}

static inline void tgtd_try_disable_out (struct tgtd *tg)
{
	struct epbase *ep = tg->owner;

	if ( (tg->pollfd.events & XPOLLOUT) ) {
		sg_update_tg (tg, tg->pollfd.events & ~XPOLLOUT);
		DEBUG_OFF ("ep %d socket %d enable pollout", ep->eid, tg->fd);
	}
}


/* Default snd/rcv buffer size = 1G */
#define SP_SNDWND 1048576000
#define SP_RCVWND 1048576000

struct epbase {
	struct epbase_vfptr vfptr;      /* the endpoint's vfptr of sp */
	union {
		u32 shutdown:1;         /* endpoint is shutdown */
		u32 bad;                /* the bad and other status bit field can share the memory */
	} status;
	atomic_t ref;                   /* reference counter */
	int eid;                        /* endpoint id, the index of sp_global.endpoints array */
	mutex_t lock;                   /* per epbase lock and condition */
	condition_t cond;
	struct msgbuf_head rcv;          /* recv buffer */
	struct msgbuf_head snd;          /* send buffer */
	struct list_head item;

	/*
	 * The next six fields are touched by walk_tgtd.  place them together
	 * so they all fit in a cache line.
	 */
	struct list_head listeners;
	u64 nlisteners;
	struct list_head connectors;
	u64 nconnectors;
	struct list_head bad_socks;
	u64 nbads;

	struct epbase_mstats stats;
};

void epbase_init (struct epbase *ep);
void epbase_exit (struct epbase *ep);

#define walk_tgtd(tg, head)				\
    walk_each_entry(tg, head, struct tgtd, item)

#define walk_tgtd_s(tg, tmp, head)			\
    walk_each_entry_s(tg, tmp, head, struct tgtd, item)

#define get_tgtd_if(tg, head, cond) ({				\
	    int ok = false;					\
	    walk_each_entry(tg, head, struct tgtd, item) {	\
		if (cond) {					\
		    ok = true;					\
		    break;					\
		}						\
	    }							\
	    if (!ok)						\
		tg = 0;						\
	    tg;							\
	})

#define rm_tgtd_if(tg, head, cond) ({			\
	if ((tg = get_tgtd_if(tg, head, cond))) {	\
	    list_del_init(&tg->item);			\
	    switch (get_socktype(tg->fd)) {		\
	    case XLISTENER:				\
		ep->nlisteners--;			\
		break;					\
	    case XCONNECTOR:				\
		ep->nconnectors--;			\
		break;					\
	    default:					\
		BUG_ON(1);				\
	    }						\
	    tg;						\
	})

int epbase_add_tgtd (struct epbase *ep, struct tgtd *tg);


#define MAX_ENDPOINTS 10240

struct sp_global {
	int exiting;

	/* eid pool lock, protect endpoints/unused/nendpoints */
	mutex_t lock;

	/* The global table of existing ep. The descriptor representing
	 * the ep is the index to this table. This pointer is also used to
	 * find out whether context is initialised. If it is null, context is
	 * uninitialised.
	 */
	struct epbase *endpoints[MAX_ENDPOINTS];

	int unused[MAX_ENDPOINTS];          /* Stack of unused ep descriptors.  */
	size_t nendpoints;                  /* Number of actual ep. */

	int pollid;
	thread_t runner;                    /* backend events_hndl runner */
	struct list_head epbase_head;       /* epbase_vfptrs head */
	struct list_head shutdown_head;     /* shutdown the epbase asyncronous */
};

extern struct sp_global sg;

#define walk_epbase_s(ep, tmp, head)				\
    walk_each_entry_s(ep, tmp, head, struct epbase, item)

#define walk_epbase_vfptr(ep, tmp, head)			\
    walk_each_entry_s(ep, tmp, head, struct epbase_vfptr, item)

static inline
struct epbase_vfptr *epbase_vfptr_lookup (int sp_family, int sp_type) {
	struct epbase_vfptr *vfptr, *tmp;

	walk_epbase_vfptr (vfptr, tmp, &sg.epbase_head) {
		if (vfptr->sp_family == sp_family && vfptr->sp_type == sp_type)
			return vfptr;
	}
	return 0;
}

extern int eid_alloc (int sp_family, int sp_type);

extern struct epbase *eid_get (int eid);

extern void eid_put (int eid);


typedef int (*ep_setopt) (struct epbase *ep, void *optval, int optlen);
typedef int (*ep_getopt) (struct epbase *ep, void *optval, int *optlen);



static inline char *clone_ubuf (char *src)
{
	int rc;
	char *dst = ubuf_alloc (ubuf_len (src));
	
	BUG_ON (!dst);
	memcpy (dst, src, ubuf_len (src));

	/* copy the sub msgbuf into dst */
	rc = ubufctl (src, SCOPY, dst);
	BUG_ON (rc);
	return dst;
}




#endif
