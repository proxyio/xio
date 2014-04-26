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
#include <xio/sock.h>
#include "xmsg.h"


#define null NULL

#define XPF_TCP       TP_TCP
#define XPF_IPC       TP_IPC
#define XPF_INPROC    TP_INPROC

/* Multiple protocols */
#define XPF_MULE      (XPF_TCP|XPF_IPC|XPF_INPROC)

#define XLISTENER   1
#define XCONNECTOR  2

/* Max number of concurrent socks. */
#define XSOCK_MAX_SOCKS 10240


int xsocket(int pf, int type);
int xbind(int xd, const char *addr);


/* XSock MQ events */
#define XMQ_PUSH         0x01
#define XMQ_POP          0x02
#define XMQ_EMPTY        0x04
#define XMQ_NONEMPTY     0x08
#define XMQ_FULL         0x10
#define XMQ_NONFULL      0x20

struct xtask;
typedef void (*xtask_func) (struct xtask *ts);

struct xtask {
    xtask_func f;
    struct list_head link;
};

#define xtask_walk_safe(ts, nt, head)			\
    list_for_each_entry_safe(ts, nt, head,		\
			     struct xtask, link)


struct xsock_protocol {
    int type;
    int pf;
    int (*bind) (int xd, const char *sock);
    void (*close) (int xd);
    void (*snd_notify) (int xd, u32 events);
    void (*rcv_notify) (int xd, u32 events);
    struct list_head link;
};

extern const char *xprotocol_str[];

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

    int parent;
    struct list_head sub_socks;
    struct list_head sib_link;

    int xd;
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
    condition_t accept_cond;
    int accept_waiters;
    struct list_head request_socks;
    struct list_head rqs_link;

    union {
	/* Only for transport xsock */
	struct {
	    ev_t et;
	    struct bio in;
	    struct bio out;
	    struct io ops;
	    int fd;
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
void xsock_free(struct xsock *sx);

struct xmsg *pop_rcv(struct xsock *cn);
void push_rcv(struct xsock *cn, struct xmsg *msg);
struct xmsg *pop_snd(struct xsock *cn);
int push_snd(struct xsock *cn, struct xmsg *msg);

int push_request_sock(struct xsock *sx, struct xsock *req_sx);
struct xsock *pop_request_sock(struct xsock *sx);

void __xpoll_notify(struct xsock *cn, u32 protocol_spec);
void xpoll_notify(struct xsock *cn, u32 protocol_spec);


#endif
