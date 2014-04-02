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

/* Max number of concurrent channels. */
#define PIO_MAX_CHANNELS 10240

/* Max number of cpu core */
#define PIO_MAX_CPUS 32

/* Define channel type for listner/accepter/connector */
#define CHANNEL_LISTENER 1
#define CHANNEL_ACCEPTER 2
#define CHANNEL_CONNECTOR 3

struct channel_vf {
    int (*init) (int cd);
    void (*destroy) (int cd);
    int (*recv) (int cd, struct channel_msg **msg);
    int (*send) (int cd, struct channel_msg *msg);
    int (*setopt) (int cd, int opt, void *val, int valsz);
    int (*getopt) (int cd, int opt, void *val, int valsz);
};

extern struct channel_vf *io_channel_vfptr;
extern struct channel_vf *inproc_channel_vfptr;

struct channel {
    mutex_t lock;
    condition_t cond;
    int ty;
    int pf;
    char sock[TP_SOCKADDRLEN];
    char peer[TP_SOCKADDRLEN];
    uint64_t fasync:1;
    uint64_t fok:1;
    int parent;
    int cd;
    int pollid;
    int waiters;
    uint64_t rcv;
    uint64_t snd;
    uint64_t rcv_wnd;
    uint64_t snd_wnd;
    struct list_head rcv_head;
    struct list_head snd_head;
    struct channel_vf *vf;

    struct list_head closing_link;
    struct list_head err_link;
    struct list_head in_link;
    struct list_head out_link;

    /* Only for transport channel */
    ev_t et;
    struct bio in;
    struct bio out;
    struct io sock_ops;
    int fd;
    struct transport *tp;

    /* Reserved only for intern process channel */
    int ref;

    /* For inproc-listener */
    struct list_head new_connectors;
    struct ssmap_node listener_node;

    /* For inproc-connector and inproc-accepter (new connection) */
    struct list_head wait_item;
    struct channel *peer_channel;
};

#define list_for_each_new_connector_safe(pos, nx, head)			\
    list_for_each_entry_safe(pos, nx, head, struct channel, wait_item)

struct channel_poll {
    spin_t lock;

    /* Backend eventloop for io runner. */
    eloop_t el;

    /* Waiting for closed channel will be attached here */
    struct list_head closing_head;

    /* Channel state list CHANNEL_ERROR|CHANNEL_MSGIN|CHANNEL_MSGOUT */
    struct list_head error_head;
    struct list_head readyin_head;
    struct list_head readyout_head;
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
};

extern struct channel_global cn_global;

static inline void cn_global_lock() {
    mutex_lock(&cn_global.lock);
}

static inline void cn_global_unlock() {
    mutex_unlock(&cn_global.lock);
}


struct channel_msg_item {
    struct channel_msg msg;
    struct list_head item;
    struct channel_msghdr hdr;
};

#define list_for_each_channel_msg_safe(pos, next, head)			\
    list_for_each_entry_safe(pos, next, head, struct channel_msg_item, item)


#endif
