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

#ifndef _H_PROXYIO_SOCKBASE_
#define _H_PROXYIO_SOCKBASE_

#include <utils/base.h>
#include <utils/str_rb.h>
#include <utils/bufio.h>
#include <utils/alloc.h>
#include <utils/atomic.h>
#include <utils/mutex.h>
#include <utils/spinlock.h>
#include <utils/condition.h>
#include <utils/taskpool.h>
#include <utils/efd.h>
#include <xio/socket.h>
#include <xio/poll.h>
#include <msgbuf/msgbuf.h>
#include <msgbuf/msgbuf_head.h>
#include <ev/ev.h>
#include "log.h"
#include "stats.h"

#define null NULL

enum {
	XAF_INPROC   =   1,
	XAF_TCP      =   2,
	XAF_IPC      =   3,
	XAF_MIX      =   4,
};

/* Default snd/rcv buffer size */
extern int default_sndbuf;
extern int default_rcvbuf;

struct sockbase;
typedef int (*sock_seter) (struct sockbase *sb, void *optval, int optlen);
typedef int (*sock_geter) (struct sockbase *sb, void *optval, int *optlen);

extern const char *pf_str[];

struct pollbase;
struct pollbase_vfptr {
	void (*emit) (struct pollbase *pb, u32 events);
	void (*close) (struct pollbase *pb);
};

struct pollbase {
	struct pollbase_vfptr *vfptr;
	struct poll_fd pollfd;
	struct list_head link;
};

#define walk_pollbase_s(pb, tmp, head)				\
    walk_each_entry_s(pb, tmp, head, struct pollbase, link)

static inline
void pollbase_init (struct pollbase *pb, struct pollbase_vfptr *vfptr)
{
	pb->vfptr = vfptr;
	INIT_LIST_HEAD (&pb->link);
}

enum {
	/* Following msgbuf_head events are provided by sockbase */
	EV_SNDBUF_ADD        =     0x001,
	EV_SNDBUF_RM         =     0x002,
	EV_SNDBUF_EMPTY      =     0x004,
	EV_SNDBUF_NONEMPTY   =     0x008,
	EV_SNDBUF_FULL       =     0x010,
	EV_SNDBUF_NONFULL    =     0x020,

	EV_RCVBUF_ADD        =     0x101,
	EV_RCVBUF_RM         =     0x102,
	EV_RCVBUF_EMPTY      =     0x104,
	EV_RCVBUF_NONEMPTY   =     0x108,
	EV_RCVBUF_FULL       =     0x110,
	EV_RCVBUF_NONFULL    =     0x120,
};

struct sockbase;
struct sockbase_vfptr {
	int type;
	int pf;
	struct sockbase * (*alloc) ();
	void  (*signal) (struct sockbase *sb, int signo);
	void  (*close)  (struct sockbase *sb);
	int   (*send)   (struct sockbase *sb, char *ubuf);
	int   (*bind)   (struct sockbase *sb, const char *sock);
	int   (*setopt) (struct sockbase *sb, int opt, void *optval, int optlen);
	int   (*getopt) (struct sockbase *sb, int opt, void *optval, int *optlen);
	struct list_head item;
};

struct sockbase {
	struct sockbase_vfptr *vfptr;
	mutex_t lock;
	condition_t cond;
	int fd;
	atomic_t ref;
	char addr[PATH_MAX];
	char peer[PATH_MAX];
	struct {
		u64 non_block:1;
		u64 epipe:1;
		u64 verbose:4;
	} flagset;
	struct ev_sig sig;
	struct ev_loop *evl;

	struct sockbase *owner;
	struct list_head sub_socks;
	struct list_head sib_link;
	struct socket_mstats stats;

	struct msgbuf_head rcv;
	struct msgbuf_head snd;

	struct {
		condition_t cond;
		int waiters;
		struct list_head head;
		struct list_head link;
	} acceptq;
	struct list_head poll_entries;
};

#define walk_sub_sock(sub, nx, head)				\
    walk_each_entry_s(sub, nx, head, struct sockbase, sib_link)


int xalloc (int family, int socktype);
struct sockbase *xget (int fd);
void xput (int fd);
void sockbase_init (struct sockbase *sb);
void sockbase_exit (struct sockbase *sb);

int rcv_msgbuf_head_add (struct sockbase *sb, struct msgbuf *msg);
struct msgbuf *rcv_msgbuf_head_rm (struct sockbase *sb);

int snd_msgbuf_head_add (struct sockbase *sb, struct msgbuf *msg);
struct msgbuf *snd_msgbuf_head_rm (struct sockbase *sb);


int acceptq_add (struct sockbase *sb, struct sockbase *new);
int acceptq_rm (struct sockbase *sb, struct sockbase **new);
int acceptq_rm_nohup (struct sockbase *sb, struct sockbase **new);

int xsocket (int pf, int socktype);
int xbind (int fd, const char *sockaddr);
void __xclose (struct sockbase *sb);

static inline int add_pollbase (int fd, struct pollbase *pb)
{
	struct sockbase *sb = xget (fd);

	if (!sb) {
		errno = EBADF;
		return -1;
	}
	mutex_lock (&sb->lock);
	BUG_ON (attached (&pb->link) );
	list_add_tail (&pb->link, &sb->poll_entries);
	mutex_unlock (&sb->lock);
	xput (fd);
	return 0;
}

int check_pollevents (struct sockbase *sb, int events);
void __emit_pollevents (struct sockbase *sb);
void emit_pollevents (struct sockbase *sb);


/* Max number of concurrent socks. */
#define PROXYIO_MAX_SOCKS 10240

struct xglobal {
	mutex_t lock;

	/* The global table of existing xsock. The descriptor representing
	 * the xsock is the index to this table. This pointer is also used to
	 * find out whether context is initialised. If it is null, context is
	 * uninitialised.
	 */
	struct sockbase *sockbases[PROXYIO_MAX_SOCKS];

	/* Stack of unused xsock descriptors.  */
	int unused[PROXYIO_MAX_SOCKS];

	/* Number of actual socks. */
	size_t nsockbases;

	/* INPROC global listening address mapping */
	struct str_rb inproc_listeners;

	/* Sockbase_vfptr head */
	struct list_head sockbase_vfptr_head;
};

#define walk_sockbase_vfptr_s(pos, nx, head)				\
    walk_each_entry_s(pos, nx, head, struct sockbase_vfptr, item)

struct sockbase_vfptr *sockbase_vfptr_lookup (int pf, int type);

extern struct xglobal xgb;




#endif
