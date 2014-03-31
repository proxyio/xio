#ifndef _HPIO_CHANNELBASE_
#define _HPIO_CHANNELBASE_


#include "os/epoll.h"
#include "bufio/bio.h"
#include "os/memory.h"
#include "hash/crc.h"
#include "sync/mutex.h"
#include "sync/spin.h"
#include "sync/condition.h"
#include "runner/taskpool.h"

/* Max number of concurrent channels. */
#define PIO_MAX_CHANNELS 10240

/* Max number of cpu core */
#define PIO_MAX_CPUS 32

/* Backend poller wait kernel timeout msec */
#define PIO_POLLER_TIMEOUT 1

/* Default input/output buffer size */
static int PIO_SNDBUFSZ = 10485760;
static int PIO_RCVBUFSZ = 10485760;


struct channel_vf {
    void (*init) (int cd);
    void (*destroy) (int cd);
    int (*accept) (int cd);
    void (*close) (int cd);
    int (*recv) (int cd, struct channel_msg **msg);
    int (*send) (int cd, struct channel_msg *msg);
    int (*setopt) (int cd, int opt, void *val, int valsz);
    int (*getopt) (int cd, int opt, void *val, int valsz);
};

extern struct channel_vf *io_channel_vfptr;
extern struct channel_vf *act_channel_vfptr;

struct channel {
    uint64_t fasync:1;
    uint64_t fok:1;
    uint64_t faccepter:1;
    int cd;
    mutex_t lock;
    uint64_t rcv_bufsz;
    uint64_t snd_bufsz;
    struct list_head rcv_head;
    struct list_head snd_head;
    condition_t cond;
    
    epollevent_t et;
    int pd;
    struct bio in;
    struct bio out;
    struct io sock_ops;
    int fd;
    struct transport *tp;
    struct list_head closing_link;
    struct list_head err_link;
    struct list_head in_link;
    struct list_head out_link;

    struct channel_vf *vf;
};

struct channel_global {
    int exiting;
    mutex_t lock;

    /*  The global table of existing channel. The descriptor representing
        the channel is the index to this table. This pointer is also used to
        find out whether context is initialised. If it is NULL, context is
        uninitialised. */
    struct channel channels[PIO_MAX_CHANNELS];

    /*  Stack of unused channel descriptors.  */
    int unused[PIO_MAX_CHANNELS];

    /*  Number of actual channels. */
    size_t nchannels;
    

    
    /*  Backend poller for io runner. */
    epoll_t polls[PIO_MAX_CPUS];

    /*  Stack of unused channel descriptors.  */
    int poll_unused[PIO_MAX_CPUS];
    
    /*  Number of actual runner poller.  */
    size_t npolls;

    struct list_head closing_head[PIO_MAX_CPUS];
    struct list_head error_head[PIO_MAX_CPUS];
    struct list_head readyin_head[PIO_MAX_CPUS];
    struct list_head readyout_head[PIO_MAX_CPUS];

    /*  Backend cpu_cores and taskpool for io runner.  */
    int cpu_cores;
    struct taskpool tpool;
};

extern struct channel_global cn_global;


struct channel_msg_item {
    struct channel_msg msg;
    struct list_head item;
    struct channel_msghdr hdr;
};

#define list_for_each_channel_msg_safe(pos, next, head)			\
    list_for_each_entry_safe(pos, next, head, struct channel_msg_item, item)


#endif
