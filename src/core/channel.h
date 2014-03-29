#ifndef _HPIO_CHANNEL_
#define _HPIO_CHANNEL_

#include "os/epoll.h"
#include "bufio/bio.h"
#include "os/memory.h"
#include "hash/crc.h"
#include "transport/transport.h"

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

#define channel_msgiov_len(msg) ({					\
	    struct channel_msg_item *msgi =				\
		container_of(msg, struct channel_msg_item, msg);	\
	    sizeof(msgi->hdr) +						\
		msgi->hdr.payload_sz + msgi->hdr.control_sz;		\
	})

#define channel_msgiov_base(msg) ({					\
	    struct channel_msg_item *msgi =				\
		container_of(msg, struct channel_msg_item, msg);	\
	    (char *)&msgi->hdr;						\
	})

static inline struct channel_msg *channel_allocmsg(
        uint32_t payload_sz, uint32_t control_sz) {
    struct channel_msg_item *msgi;
    char *chunk = (char *)mem_zalloc(sizeof(*msgi) + payload_sz + control_sz);
    if (!chunk)
	return NULL;
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


#define CHANNEL_MSGIN 1
#define CHANNEL_MSGOUT 2
#define CHANNEL_ERROR 4

struct channel {
    struct list_head rcv_head;
    struct list_head snd_head;

    uint32_t events;
    epollevent_t et;
    epoll_t *el;
    struct bio b;
    struct io sock_ops;
    int fd;
    struct transport *tp;
};

void channel_global_init();
void channel_init(struct channel *cn, int fd, struct transport *tp);
void channel_destroy(struct channel *cn);
int channel_recv(struct channel *cn, struct channel_msg **msg);
int channel_send(struct channel *cn, const struct channel_msg *msg);



#endif
