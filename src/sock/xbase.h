#ifndef _HPIO_XSOCKBASE_
#define _HPIO_XSOCKBASE_

#include <base.h>
#include <ds/map.h>
#include <os/eventloop.h>
#include <bufio/bio.h>
#include <os/alloc.h>
#include <hash/crc.h>
#include <sync/mutex.h>
#include <sync/spin.h>
#include <sync/condition.h>
#include <runner/taskpool.h>
#include <os/efd.h>
#include <transport/transport.h>
#include <poll/xpoll_struct.h>
#include <xio/sock.h>

#define null NULL

/* Multiple protocols */
#define PF_MULE      (PF_NET|PF_IPC|PF_INPROC)

/* Max number of concurrent socks. */
#define XSOCK_MAX_SOCKS 10240

/* Max number of cpu core */
#define XSOCK_MAX_CPUS 32

/* XSock MQ events */
#define XMQ_PUSH         0x01
#define XMQ_POP          0x02
#define XMQ_EMPTY        0x04
#define XMQ_NONEMPTY     0x08
#define XMQ_FULL         0x10
#define XMQ_NONFULL      0x20

struct xtask;
typedef int (*xtask_func) (struct xtask *ts);

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






struct xcpu {
    //spin_t lock; // for release mode

    mutex_t lock; // for debug mode

    /* Backend eventloop for cpu_worker. */
    eloop_t el;

    ev_t efd_et;
    struct efd efd;

    /* Waiting for closed xsock will be attached here */
    struct list_head shutdown_socks;
};

int xcpu_alloc();
void xcpu_free(int cpu_no);
struct xcpu *xcpuget(int cpu_no);

struct xglobal {
    int exiting;
    mutex_t lock;

    /* The global table of existing xsock. The descriptor representing
       the xsock is the index to this table. This pointer is also used to
       find out whether context is initialised. If it is null, context is
       uninitialised. */
    struct xsock socks[XSOCK_MAX_SOCKS];

    /* Stack of unused xsock descriptors.  */
    int unused[XSOCK_MAX_SOCKS];

    /* Number of actual socks. */
    size_t nsocks;
    

    struct xcpu cpus[XSOCK_MAX_CPUS];

    /* Stack of unused xsock descriptors.  */
    int cpu_unused[XSOCK_MAX_CPUS];
    
    /* Number of actual runner poller.  */
    size_t ncpus;

    /* Backend cpu_cores and taskpool for cpu_worker.  */
    int cpu_cores;
    struct taskpool tpool;

    /* INPROC global listening address mapping */
    struct ssmap inproc_listeners;

    /* xsock_protocol head */
    struct list_head xsock_protocol_head;
};

#define xsock_protocol_walk_safe(pos, nx, head)			\
    list_for_each_entry_safe(pos, nx, head,			\
			     struct xsock_protocol, link)

struct xsock_protocol *l4proto_lookup(int pf, int type);

extern struct xglobal xgb;

static inline void xglobal_lock() {
    mutex_lock(&xgb.lock);
}

static inline void xglobal_unlock() {
    mutex_unlock(&xgb.lock);
}




/*
  The transport protocol header is 6 bytes long and looks like this:

  +--------+------------+------------+
  | 0xffff | 0xffffffff | 0xffffffff |
  +--------+------------+------------+
  |  crc16 |    size    |    chunk   |
  +--------+------------+------------+
*/

struct xiov {
    uint16_t checksum;
    u32 size;
    char chunk[0];
};

struct xmsg {
    struct list_head item;
    struct xiov vec;
};

u32 xiov_len(char *xbuf);
char *xiov_base(char *xbuf);

#define xmsg_walk_safe(pos, next, head)					\
    list_for_each_entry_safe(pos, next, head, struct xmsg, item)


#endif