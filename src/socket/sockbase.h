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
#include <utils/map.h>
#include <utils/eventloop.h>
#include <utils/bufio.h>
#include <utils/alloc.h>
#include <utils/atomic.h>
#include <utils/mutex.h>
#include <utils/spinlock.h>
#include <utils/condition.h>
#include <utils/taskpool.h>
#include <utils/transport.h>
#include <xio/socket.h>
#include <xio/poll.h>
#include "skbuf.h"
#include "worker.h"
#include "stats.h"

#define null NULL

/* Default snd/rcv buffer size */
extern int default_sndbuf;
extern int default_rcvbuf;

/* Sockspec_vfptr notify types */
#define RECV_Q           1
#define SEND_Q           2
#define SOCKS_REQ        3

/* Following xmq events are provided by sockbase */
#define XMQ_PUSH         0x01
#define XMQ_POP          0x02
#define XMQ_EMPTY        0x04
#define XMQ_NONEMPTY     0x08
#define XMQ_FULL         0x10
#define XMQ_NONFULL      0x20

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



struct sockbase;
struct sockbase_vfptr {
	int type;
	int pf;
	struct sockbase * (*alloc) ();
	void  (*close)  (struct sockbase *sb);
	int   (*bind)   (struct sockbase *sb, const char *sock);
	void  (*notify) (struct sockbase *sb, int type, u32 events);
	int   (*setopt) (struct sockbase *sb, int level, int opt, void *optval,
	                 int optlen);
	int   (*getopt) (struct sockbase *sb, int level, int opt, void *optval,
	                 int *optlen);
	struct list_head link;
};

struct sockbase {
	struct sockbase_vfptr *vfptr;
	mutex_t lock;
	condition_t cond;
	int fd;
	atomic_t ref;
	int cpu_no;
	char addr[TP_SOCKADDRLEN];
	char peer[TP_SOCKADDRLEN];
	u64 fasync:1;
	u64 fepipe:1;

	struct sockbase *owner;
	struct list_head sub_socks;
	struct list_head sib_link;
	struct socket_mstats stats;

	struct {
		int waiters;
		int wnd;
		int buf;
		struct list_head head;
	} rcv;

	struct {
		int waiters;
		int wnd;
		int buf;
		struct list_head head;
	} snd;

	struct {
		condition_t cond;
		int waiters;
		struct list_head head;
		struct list_head link;
	} acceptq;

	struct task_ent shutdown;
	struct list_head poll_entries;
};

#define walk_sub_sock(sub, nx, head)				\
    walk_each_entry_s(sub, nx, head, struct sockbase, sib_link)

/* We guarantee that we can push one massage at least. */
static inline int can_send (struct sockbase *sb)
{
	return list_empty (&sb->snd.head) || sb->snd.buf < sb->snd.wnd;
}

static inline int can_recv (struct sockbase *sb)
{
	return list_empty (&sb->rcv.head) || sb->rcv.buf < sb->rcv.wnd;
}

int xalloc (int family, int socktype);
struct sockbase *xget (int fd);
void xput (int fd);
void sockbase_init (struct sockbase *sb);
void sockbase_exit (struct sockbase *sb);

int recvq_add (struct sockbase *sb, struct skbuf *msg);
struct skbuf *recvq_rm (struct sockbase *sb);

int sendq_add (struct sockbase *sb, struct skbuf *msg);
struct skbuf *sendq_rm (struct sockbase *sb);


int acceptq_add (struct sockbase *sb, struct sockbase *new);
int acceptq_rm (struct sockbase *sb, struct sockbase **new);
int acceptq_rm_nohup (struct sockbase *sb, struct sockbase **new);

/* Following socktype are provided */
#define XLISTENER       1
#define XCONNECTOR      2

int xsocket (int pf, int socktype);
int xbind (int fd, const char *sockaddr);


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


#endif
