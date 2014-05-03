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

#ifndef _HPIO_XSOCK_STRUCT_
#define _HPIO_XSOCK_STRUCT_

#include <base.h>
#include <ds/map.h>
#include <os/eventloop.h>
#include <bufio/bio.h>
#include <os/alloc.h>
#include <sync/mutex.h>
#include <sync/spin.h>
#include <sync/condition.h>
#include <runner/taskpool.h>
#include <transport/transport.h>
#include <poll/xpoll_struct.h>
#include <xio/socket.h>
#include "xmsg.h"
#include "xcpu.h"
#include "xprotocol.h"

#define null NULL

#define XPF_TCP       TP_TCP
#define XPF_IPC       TP_IPC
#define XPF_INPROC    TP_INPROC

/* Multiple protocols */
#define XPF_MULE      (XPF_TCP|XPF_IPC|XPF_INPROC)

/* Max number of concurrent socks. */
#define XIO_MAX_SOCKS 10240


int xsocket(int pf, int type);
int xbind(int fd, const char *addr);

/* xsock_protocol notify types */
#define RECV_Q           1
#define SEND_Q           2
#define SOCKS_REQ        3

/* Following xmq events are provided by xsock */
#define XMQ_PUSH         0x01
#define XMQ_POP          0x02
#define XMQ_EMPTY        0x04
#define XMQ_NONEMPTY     0x08
#define XMQ_FULL         0x10
#define XMQ_NONFULL      0x20

struct xsock {
    mutex_t lock;
    condition_t cond;
    int type;
    int pf;

    char addr[TP_SOCKADDRLEN];
    char peer[TP_SOCKADDRLEN];
    u64 fasync:1;
    u64 fok:1;
    u64 fclosed:1;
    u64 ftracedebug:1;

    int owner;
    struct list_head sub_socks;
    struct list_head sib_link;

    int fd;
    int cpu_no;
    int rcv_waiters;
    int snd_waiters;
    u64 rcv;
    u64 snd;
    u64 rcv_wnd;
    u64 snd_wnd;
    struct list_head rcv_head;
    struct list_head snd_head;
    struct xsock_protocol *l4proto;
    struct list_head xpoll_head;
    struct xtask shutdown;

    struct {
	condition_t cond;
	int waiters;
	struct list_head head;
	struct list_head link;
    } acceptq;

    union {
	/* Only for transport xsock */
	struct {
	    ev_t et;
	    struct bio in;
	    struct bio out;
	    struct io ops;
	    int sys_fd;
	    struct transport *tp;
	} io;

	/* Reserved only for intern process xsock */
	struct {
	    /* Reference by self and the peer. in normal case
	     * if ref == 2, connection work in normal state
	     * if ref == 1, connection closed by peer.
	     * if ref == 0, is should destroy because no other hold it.
	     */
	    int ref;

	    /* For inproc-listener */
	    struct ssmap_node rb_link;

	    /* For inproc-connector and inproc-accepter (new connection) */
	    struct xsock *xsock_peer;
	} proc;
    };
};

#define xsock_walk_sub_socks(sub, nx, head)		\
    list_for_each_entry_safe(sub, nx, head,		\
			     struct xsock, sib_link)

/* We guarantee that we can push one massage at least. */
static inline int can_send(struct xsock *cn) {
    return list_empty(&cn->snd_head) || cn->snd < cn->snd_wnd;
}

static inline int can_recv(struct xsock *cn) {
    return list_empty(&cn->rcv_head) || cn->rcv < cn->rcv_wnd;
}

struct xsock *xget(int cd);
void xsock_free(struct xsock *cn);
struct xsock *xsock_alloc();

void recvq_push(struct xsock *cn, struct xmsg *msg);
struct xmsg *sendq_pop(struct xsock *cn);

int acceptq_push(struct xsock *xsk, struct xsock *req_xsk);
struct xsock *acceptq_pop(struct xsock *xsk);

int xpoll_check_events(struct xsock *xsk, int events);
void __xpoll_notify(struct xsock *xsk);
void xpoll_notify(struct xsock *xsk);


#endif
