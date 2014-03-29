#ifndef _HPIO_CHANNEL_
#define _HPIO_CHANNEL_

#include "os/epoll.h"
#include "bufio/bio.h"
#include "os/memory.h"
#include "hash/crc.h"
#include "transport.h"
#include "sync/mutex.h"
#include "sync/spin.h"


/*
  The transport protocol header is 10 bytes long and looks like this:

  +--------+------------+------------+
  | 0xffff | 0xffffffff | 0xffffffff |
  +--------+------------+------------+
  |  crc16 |    ctlsz   | payloadlen |
  +--------+------------+------------+
*/

struct channel_msghdr {
    uint16_t checksum;
    uint32_t payload_sz;
    uint32_t control_sz;
};

struct channel_msg {
    char *payload;
    char *control;
};

struct channel_msg_item {
    struct channel_msg msg;
    struct list_head item;
    struct channel_msghdr hdr;
};

#define list_for_each_channel_msg_safe(pos, next, head)			\
    list_for_each_entry_safe(pos, next, head, struct channel_msg_item, item)

#define channel_msgiov_len(ptr) ({					\
	    struct channel_msg_item *msgi =				\
		container_of((ptr), struct channel_msg_item, msg);	\
	    sizeof(msgi->hdr) +						\
		msgi->hdr.payload_sz + msgi->hdr.control_sz;		\
	})

#define channel_msgiov_base(ptr) ({					\
	    struct channel_msg_item *msgi =				\
		container_of((ptr), struct channel_msg_item, msg);	\
	    (char *)&msgi->hdr;						\
	})

static inline struct channel_msg *channel_allocmsg(
        uint32_t payload_sz, uint32_t control_sz) {
    struct channel_msg_item *msgi;
    char *chunk = (char *)mem_zalloc(sizeof(*msgi) + payload_sz + control_sz);
    if (!chunk)
	return NULL;
    msgi = (struct channel_msg_item *)chunk;
    msgi->hdr.payload_sz = payload_sz;
    msgi->hdr.control_sz = control_sz;
    msgi->hdr.checksum = crc16((char *)&msgi->hdr.payload_sz, 16);
    msgi->msg.payload = chunk + sizeof(*msgi);
    msgi->msg.control = msgi->msg.payload + payload_sz;
    return &msgi->msg;
}

static inline void channel_freemsg(struct channel_msg *msg) {
    struct channel_msg_item *msgi = container_of(msg, struct channel_msg_item, msg);
    mem_free(msgi, sizeof(*msgi) + msgi->hdr.payload_sz + msgi->hdr.control_sz);
}






/*  Max number of concurrent channels. */
#define PIO_MAX_CHANNELS 10240

struct channel {
    spin_t lock;
    struct list_head rcv_head;
    struct list_head snd_head;

    epollevent_t et;
    int pd;
    struct bio in;
    struct bio out;
    struct io sock_ops;
    int fd;
    struct transport *tp;
    struct list_head poller_item;
};

struct channel_global {
    mutex_t lock;
    
    /*  The global table of existing channel. The descriptor representing
        the channel is the index to this table. This pointer is also used to
        find out whether context is initialised. If it is NULL, context is
        uninitialised. */
    struct channel channels[PIO_MAX_CHANNELS];

    /*  Stack of unused file descriptors. */
    int unused[PIO_MAX_CHANNELS];

    /*  Number of actual open channels in the channel table. */
    size_t nchannels;

    /*  Combination of the flags listed above. */
    int flags;

    /*  List of all available transports. */
    struct list_head transport_head;
};


void channel_global_init();


int channel_listen(int pf, const char *sock);
int channel_connect(int pf, const char *peer);
int channel_accept(int cd);
int channel_recv(int cd, struct channel_msg **msg);
int channel_send(int cd, const struct channel_msg *msg);
int channel_setopt(int cd, int opt, void *val, int valsz);
int channel_getopt(int cd, int opt, void *val, int valsz);
void channel_close(int cd);

#endif
