#ifndef _HPIO_CHANNELBASE_
#define _HPIO_CHANNELBASE_

#include "ds/map.h"
#include "os/eventloop.h"
#include "bufio/bio.h"
#include "os/alloc.h"
#include "hash/crc.h"
#include "sync/mutex.h"
#include "sync/spin.h"
#include "sync/condition.h"
#include "runner/taskpool.h"
#include "xsock.h"
#include "xpoll.h"

/* Max number of concurrent socks. */
#define PIO_MAX_SOCKS 10240

/* Max number of cpu core */
#define PIO_MAX_CPUS 32

/* Define channel type for listner/accepter/connector */
#define XLISTENER 1
#define XACCEPTER 2
#define XCONNECTOR 3

#define MQ_PUSH         0x01
#define MQ_POP          0x02
#define MQ_EMPTY        0x04
#define MQ_NONEMPTY     0x08
#define MQ_FULL         0x10
#define MQ_NONFULL      0x20

struct xsock_vf {
    int pf;
    int (*init) (int xd);
    void (*destroy) (int xd);
    /* The lock is hold by the caller */
    void (*snd_notify) (int xd, u32 events);
    void (*rcv_notify) (int xd, u32 events);
    struct list_head vf_item;
};


struct xsock {
    mutex_t lock;
    condition_t cond;
    int ty;
    int pf;
    char addr[TP_SOCKADDRLEN];
    char peer[TP_SOCKADDRLEN];
    uint64_t fasync:1;
    uint64_t fok:1;
    uint64_t fclosed:1;
    int parent;
    int xd;
    int cpu_no;
    int rcv_waiters;
    int snd_waiters;
    uint64_t rcv;
    uint64_t snd;
    uint64_t rcv_wnd;
    uint64_t snd_wnd;
    struct list_head rcv_head;
    struct list_head snd_head;
    struct xsock_vf *vf;
    struct list_head closing_link;
    struct list_head xpoll_head;

    /* Only for transport channel */
    struct {
	ev_t et;
	struct bio in;
	struct bio out;
	struct io ops;
	int fd;
	struct transport *tp;
    } sock;

    /* Reserved only for intern process channel */
    struct {
	/* Reference by self and the peer. in normal case
	 * if ref == 2, connection work in normal state
	 * if ref == 1, connection closed by peer.
	 * if ref == 0, is should destroy because no other hold it.
	 */
	int ref;

	/* For inproc-listener */
	struct list_head new_connectors;
	struct ssmap_node listener_node;

	/* For inproc-connector and inproc-accepter (new connection) */
	struct list_head wait_item;
	struct xsock *peer_channel;
    } proc;
};


// We guarantee that we can push one massage at least.
static inline int can_send(struct xsock *cn) {
    return list_empty(&cn->snd_head) || cn->snd < cn->snd_wnd;
}

static inline int can_recv(struct xsock *cn) {
    return list_empty(&cn->rcv_head) || cn->rcv < cn->rcv_wnd;
}

#define xsock_walk_safe(pos, nx, head)			\
    list_for_each_entry_safe(pos, nx, head,		\
			     struct xsock, wait_item)


struct xcpu {
    spin_t lock;

    /* Backend eventloop for io runner. */
    eloop_t el;

    /* Waiting for closed channel will be attached here */
    struct list_head closing_head;
};


struct xglobal {
    int exiting;
    mutex_t lock;

    /* The global table of existing channel. The descriptor representing
       the channel is the index to this table. This pointer is also used to
       find out whether context is initialised. If it is NULL, context is
       uninitialised. */
    struct xsock socks[PIO_MAX_SOCKS];

    /* Stack of unused channel descriptors.  */
    int unused[PIO_MAX_SOCKS];

    /* Number of actual socks. */
    size_t nsocks;
    

    struct xcpu cpus[PIO_MAX_CPUS];

    /* Stack of unused channel descriptors.  */
    int cpu_unused[PIO_MAX_CPUS];
    
    /* Number of actual runner poller.  */
    size_t ncpus;

    /* Backend cpu_cores and taskpool for io runner.  */
    int cpu_cores;
    struct taskpool tpool;

    /* INPROC global listening address mapping */
    struct ssmap inproc_listeners;

    /* Channel vfptr head */
    struct list_head xsock_vf_head;
};

struct xsock *xget(int xd);

#define xsock_vf_walk_safe(pos, nx, head)				\
    list_for_each_entry_safe(pos, nx, head, struct xsock_vf, vf_item)



extern struct xglobal xglobal;

#define global_vf_head &xglobal.xsock_vf_head
#define global_tpool &xglobal.tpool
#define global_inplistenrs &xglobal.inproc_listeners


static inline void xglobal_lock() {
    mutex_lock(&xglobal.lock);
}

static inline void xglobal_unlock() {
    mutex_unlock(&xglobal.lock);
}




/*
  The transport protocol header is 6 bytes long and looks like this:

  +--------+------------+------------+
  | 0xffff | 0xffffffff | 0xffffffff |
  +--------+------------+------------+
  |  crc16 |    size    |   payload  |
  +--------+------------+------------+
*/

struct xmsghdr {
    uint16_t checksum;
    u32 size;
    char payload[0];
};

struct xmsg {
    struct list_head item;
    struct xmsghdr hdr;
};

u32 msg_iovlen(char *payload);
char *msg_iovbase(char *payload);

#define xmsg_walk_safe(pos, next, head)					\
    list_for_each_entry_safe(pos, next, head, struct xmsg, item)


#endif
