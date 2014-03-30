#ifndef _HPIO_CHANNEL_
#define _HPIO_CHANNEL_

#include "os/epoll.h"
#include "bufio/bio.h"
#include "os/memory.h"
#include "hash/crc.h"
#include "transport/transport.h"
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
	    struct channel_msg_item *_msgi =				\
		container_of((ptr), struct channel_msg_item, msg);	\
	    sizeof(_msgi->hdr) +						\
		_msgi->hdr.payload_sz + _msgi->hdr.control_sz;		\
	})

#define channel_msgiov_base(ptr) ({					\
	    struct channel_msg_item *_msgi =				\
		container_of((ptr), struct channel_msg_item, msg);	\
	    (char *)&_msgi->hdr;						\
	})


struct channel_msg *channel_allocmsg(uint32_t payload_sz, uint32_t control_sz);
void channel_freemsg(struct channel_msg *msg);


int channel_listen(int pf, const char *sock);
int channel_connect(int pf, const char *peer);
int channel_accept(int cd);
int channel_recv(int cd, struct channel_msg **msg);
int channel_send(int cd, struct channel_msg *msg);
int channel_setopt(int cd, int opt, void *val, int valsz);
int channel_getopt(int cd, int opt, void *val, int valsz);
void channel_close(int cd);

#endif
