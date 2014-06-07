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

#ifndef _XIO_SP_MODULE_
#define _XIO_SP_MODULE_

#include <uuid/uuid.h>
#include <utils/list.h>
#include <utils/mutex.h>
#include <utils/atomic.h>
#include <utils/spinlock.h>
#include <utils/condition.h>
#include <utils/thread.h>
#include <socket/xgb.h>
#include <xio/poll.h>
#include <xio/sp_reqrep.h>
#include "sp_hdr.h"


struct epbase;
struct socktg;
struct epbase_vfptr {
    int sp_family;
    int sp_type;
    struct epbase *(*alloc) ();
    void (*destroy) (struct epbase *ep);
    int  (*send)     (struct epbase *ep, char *ubuf);
    int  (*rm)      (struct epbase *ep, struct socktg *sk, char **ubuf);
    int  (*add)     (struct epbase *ep, struct socktg *sk, char *ubuf);
    int  (*join)    (struct epbase *ep, struct socktg *sk, int fd);
    int  (*setopt)  (struct epbase *ep, int opt, void *optval, int optlen);
    int  (*getopt)  (struct epbase *ep, int opt, void *optval, int *optlen);
    struct list_head item;
};

struct socktg *sp_generic_join(struct epbase *ep, int fd);

struct socktg {
    struct epbase *owner;
    struct poll_ent ent;
    int fd;
    uuid_t uuid;
    struct list_head item;
    struct list_head out_item;
    struct list_head snd_cache;
};

struct socktg *socktg_new();
void socktg_free(struct socktg *sk);
void sg_add_sk(struct socktg *sk);
void sg_rm_sk(struct socktg *sk);
void sg_update_sk(struct socktg *sk, u32 ev);
void __socktg_try_enable_out(struct socktg *sk);


struct skbuf {
    int wnd;
    int size;
    int waiters;
    struct list_head head;
};


#define EP_SHUTDOWN 1

struct epbase {
    struct epbase_vfptr vfptr;
    u32 status;
    atomic_t ref;
    int eid;
    mutex_t lock;
    condition_t cond;
    struct poll_ent ent;
    struct skbuf rcv;
    struct skbuf snd;
    struct list_head item;
    u64 listener_num;
    u64 connector_num;
    u64 bad_num;
    u64 disable_out_num;
    struct list_head listeners;
    struct list_head connectors;
    struct list_head bad_socks;
    struct list_head disable_pollout_socks;
};

void epbase_init(struct epbase *ep);
void epbase_exit(struct epbase *ep);

#define walk_socktg_s(tg, tmp, head)				\
    walk_each_entry_s(tg, tmp, head, struct socktg, item)

#define get_socktg_if(tg, head, cond) ({			\
	    int ok = false;					\
	    walk_each_entry(tg, head, struct socktg, item) {	\
		if (cond) {					\
		    ok = true;					\
		    break;					\
		}						\
	    }							\
	    if (!ok)						\
		tg = 0;						\
	    tg;							\
	})

#define rm_socktg_if(tg, head, cond) ({			\
	    if ((tg = get_socktg_if(tg, head, cond)))	\
		list_del_init(&tg->item);		\
	    tg;						\
	});

#define walk_disable_out_sk_s(sk, nsk, head)			\
    walk_each_entry_s(sk, nsk, head, struct socktg, out_item)



#define XIO_MAX_ENDPOINTS 10240

struct sp_global {
    int exiting;
    mutex_t lock;

    /* The global table of existing ep. The descriptor representing
     * the ep is the index to this table. This pointer is also used to
     * find out whether context is initialised. If it is null, context is
     * uninitialised.
     */
    struct epbase *endpoints[XIO_MAX_ENDPOINTS];

    /* Stack of unused ep descriptors.  */
    int unused[XIO_MAX_ENDPOINTS];

    /* Number of actual ep. */
    size_t nendpoints;

    int pollid;
    thread_t po_routine;
    struct list_head epbase_head;
    struct list_head shutdown_head;
};

extern struct sp_global sg;

#define walk_epbase_s(ep, tmp, head)				\
    walk_each_entry_s(ep, tmp, head, struct epbase, item)

#define walk_epbase_vfptr(ep, tmp, head)			\
    walk_each_entry_s(ep, tmp, head, struct epbase_vfptr, item)

static inline
struct epbase_vfptr *epbase_vfptr_lookup(int sp_family, int sp_type) {
    struct epbase_vfptr *vfptr, *tmp;

    walk_epbase_vfptr(vfptr, tmp, &sg.epbase_head) {
	if (vfptr->sp_family == sp_family && vfptr->sp_type == sp_type)
	    return vfptr;
    }
    return 0;
}

int eid_alloc(int sp_family, int sp_type);
struct epbase *eid_get(int eid);
void eid_put(int eid);


typedef int (*ep_setopt) (struct epbase *ep, void *optval, int optlen);
typedef int (*ep_getopt) (struct epbase *ep, void *optval, int *optlen);


#endif
