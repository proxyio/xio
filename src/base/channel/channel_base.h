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
#include "channel.h"
#include "poll.h"

/* Max number of concurrent channels. */
#define PIO_MAX_CHANNELS 10240

/* Max number of cpu core */
#define PIO_MAX_CPUS 32

/* Define channel type for listner/accepter/connector */
#define CHANNEL_LISTENER 1
#define CHANNEL_ACCEPTER 2
#define CHANNEL_CONNECTOR 3

#define MQ_PUSH         0x01
#define MQ_POP          0x02
#define MQ_EMPTY        0x04
#define MQ_NONEMPTY     0x08
#define MQ_FULL         0x10
#define MQ_NONFULL      0x20

struct channel_vf {
    int pf;
    int (*init) (int cd);
    void (*destroy) (int cd);

    /* The lock is hold by the caller */
    void (*snd_notify) (int cd, uint32_t events);
    void (*rcv_notify) (int cd, uint32_t events);
    struct list_head vf_item;
};


struct channel {
    mutex_t lock;
    condition_t cond;
    int ty;
    int pf;
    char addr[TP_SOCKADDRLEN];
    char peer[TP_SOCKADDRLEN];
    uint64_t fasync:1;
    uint64_t fok:1;
    int parent;
    int cd;
    int pollid;
    struct channel_events ev;
    int rcv_waiters;
    int snd_waiters;
    uint64_t rcv;
    uint64_t snd;
    uint64_t rcv_wnd;
    uint64_t snd_wnd;
    struct list_head rcv_head;
    struct list_head snd_head;
    struct channel_vf *vf;
    struct list_head upoll_entries;
    struct list_head closing_link;

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
	struct channel *peer_channel;
    } proc;
};

// We guarantee that we can push one massage at least.
static inline int can_send(struct channel *cn) {
    return list_empty(&cn->snd_head) || cn->snd < cn->snd_wnd;
}

static inline int can_recv(struct channel *cn) {
    return list_empty(&cn->rcv_head) || cn->rcv < cn->rcv_wnd;
}

#define list_for_each_new_connector_safe(pos, nx, head)			\
    list_for_each_entry_safe(pos, nx, head, struct channel, wait_item)


void channel_add_upoll_entry(struct upoll_entry *ent);
void channel_rm_upoll_entry(struct upoll_entry *ent);



struct channel_poll {
    spin_t lock;

    /* Backend eventloop for io runner. */
    eloop_t el;

    /* Waiting for closed channel will be attached here */
    struct list_head closing_head;
};


struct channel_global {
    int exiting;
    mutex_t lock;

    /* The global table of existing channel. The descriptor representing
       the channel is the index to this table. This pointer is also used to
       find out whether context is initialised. If it is NULL, context is
       uninitialised. */
    struct channel channels[PIO_MAX_CHANNELS];

    /* Stack of unused channel descriptors.  */
    int unused[PIO_MAX_CHANNELS];

    /* Number of actual channels. */
    size_t nchannels;
    

    struct channel_poll polls[PIO_MAX_CPUS];

    /* Stack of unused channel descriptors.  */
    int poll_unused[PIO_MAX_CPUS];
    
    /* Number of actual runner poller.  */
    size_t npolls;

    /* Backend cpu_cores and taskpool for io runner.  */
    int cpu_cores;
    struct taskpool tpool;

    /* INPROC global listening address mapping */
    struct ssmap inproc_listeners;

    /* Channel vfptr head */
    struct list_head channel_vf_head;
};

#define list_for_each_channel_vf_safe(pos, nx, head)			\
    list_for_each_entry_safe(pos, nx, head, struct channel_vf, vf_item)



extern struct channel_global cn_global;

#define global_vf_head &cn_global.channel_vf_head
#define global_tpool &cn_global.tpool
#define global_inplistenrs &cn_global.inproc_listeners


static inline void cn_global_lock() {
    mutex_lock(&cn_global.lock);
}

static inline void cn_global_unlock() {
    mutex_unlock(&cn_global.lock);
}




/*
  The transport protocol header is 6 bytes long and looks like this:

  +--------+------------+------------+
  | 0xffff | 0xffffffff | 0xffffffff |
  +--------+------------+------------+
  |  crc16 |    size    |   payload  |
  +--------+------------+------------+
*/

struct channel_msghdr {
    uint16_t checksum;
    uint32_t size;
    char payload[0];
};

struct channel_msg {
    struct list_head item;
    struct channel_msghdr hdr;
};

uint32_t msg_iovlen(char *payload);
char *msg_iovbase(char *payload);

#define list_for_each_channel_msg_safe(pos, next, head)			\
    list_for_each_entry_safe(pos, next, head, struct channel_msg, item)


#endif
