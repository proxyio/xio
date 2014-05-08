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
#include <xio/socket.h>
#include <poll/xeventpoll.h>
#include "xmsg.h"
#include "xcpu.h"

#define null NULL

/* Multiple protocols */
#define XPF_MULE      (XPF_TCP|XPF_IPC|XPF_INPROC)

/* Max number of concurrent socks. */
#define XIO_MAX_SOCKS 10240


int xsocket(int pf, int type);
int xbind(int fd, const char *addr);

/* Sockspec_vfptr notify types */
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

extern const char *pf_str[];

struct sockbase_vfptr {
    int type;
    int pf;
    int (*bind) (int fd, const char *sock);
    void (*close) (int fd);
    void (*notify) (int fd, int type, u32 events);
    int (*setopt) (int fd, int level, int opt, void *optval, int optlen);
    int (*getopt) (int fd, int level, int opt, void *optval, int *optlen);
    struct list_head link;
};

struct sockbase {
    struct sockbase_vfptr *vfptr;
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
    int ref;
    int cpu_no;

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

    struct xtask shutdown;
    struct list_head poll_entries;

    union {
	/* Only for tcp/ipc socket */
	struct {
	    ev_t et;
	    struct bio in;
	    struct bio out;
	    struct io ops;
	    int sys_fd;
	    struct transport *tp;
	} io;

	/* Reserved only for intern process socket*/
	struct {
	    /* For inproc-listener */
	    struct ssmap_node rb_link;

	    /* For inproc-connector and inproc-accepter */
	    struct sockbase *xsock_peer;
	} proc;
    };
};

#define xsock_walk_sub_socks(sub, nx, head)		\
    list_for_each_entry_safe(sub, nx, head,		\
			     struct sockbase, sib_link)

/* We guarantee that we can push one massage at least. */
static inline int can_send(struct sockbase *cn) {
    return list_empty(&cn->snd.head) || cn->snd.buf < cn->snd.wnd;
}

static inline int can_recv(struct sockbase *cn) {
    return list_empty(&cn->rcv.head) || cn->rcv.buf < cn->rcv.wnd;
}

struct sockbase *xget(int cd);
void xsock_free(struct sockbase *cn);
struct sockbase *xsock_alloc();

void recvq_push(struct sockbase *cn, struct xmsg *msg);
struct xmsg *sendq_pop(struct sockbase *cn);

int acceptq_push(struct sockbase *self, struct sockbase *req_self);
struct sockbase *acceptq_pop(struct sockbase *self);

int xpoll_check_events(struct sockbase *self, int events);
void __xeventnotify(struct sockbase *self);
void xeventnotify(struct sockbase *self);


#endif
