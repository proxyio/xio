#ifndef _HPIO_TRANSPORT_
#define _HPIO_TRANSPORT_

#include "os/memory.h"
#include "hash/crc.h"

#define CHANNEL_LINGER 1
#define CHANNEL_SNDBUF 2
#define CHANNEL_RCVBUF 3
#define CHANNEL_SNDTIMEO 4
#define CHANNEL_RCVTIMEO 5

/*
  The transport protocol header is 10 bytes long and looks like this:

  +--------+------------+------------+
  | 0xffff | 0xffffffff | 0xffffffff |
  +--------+------------+------------+
  |  crc16 |    ctlsz   | payloadlen |
  +--------+------------+------------+
*/

#define PIO_MSG -1

struct channel_msghdr {
    uint16_t checksum;
    uint32_t payload_sz;
    uint32_t control_sz;
};

struct channel_msg {
    char *payload;
    char *control;
};

static inline struct channel_msg *channel_allocmsg(
        uint32_t payload_sz, uint32_t control_sz) {
    struct channel_msghdr *hdr;
    struct channel_msg *msg;
    int chunk_size = sizeof(*msg) + sizeof(*hdr) + payload_sz + control_sz;
    char *chunk = (char *)mem_zalloc(chunk_size);
    if (!chunk)
	return NULL;
    hdr = (struct channel_msghdr *)(chunk + sizeof(*msg));
    hdr->payload_sz = payload_sz;
    hdr->control_sz = control_sz;
    hdr->checksum = crc16((char *)&hdr->payload_sz, 16);
    msg = (struct channel_msg *)chunk;
    msg->payload = chunk + sizeof(*msg) + sizeof(*hdr);
    msg->control = msg->payload + payload_sz;
    return msg;
}

static inline void channel_freemsg(struct channel_msg *msg) {
    // TODO: fix me!
    mem_free(msg, 0);
}

struct channel {
    void (*close) (struct channel *cn);
    int (*send) (struct channel *cn, const struct channel_msg *msg);
    int (*recv) (struct channel *cn, struct channel_msg **msg);
};


#define PIO_TCP -1
#define PIO_IPC -2
#define PIO_INPROC -3

#define PIO_LINGER 1
#define PIO_SNDBUF 2
#define PIO_RCVBUF 3
#define PIO_NONBLOCK 4
#define PIO_NODELAY 5
#define PIO_RCVTIMEO 6
#define PIO_SNDTIMEO 7
#define PIO_REUSEADDR 8
#define PIO_SOCKADDRLEN 4096

struct transport {
    const char *name;
    int proto;
    void (*close) (int fd);
    int (*bind) (const char *sock);
    int (*accept) (int fd);
    int (*connect) (const char *peer);
    int64_t (*read) (int fd, char *buff, int64_t size);
    int64_t (*write) (int fd, const char *buff, int64_t size);
    int (*setopt) (int fd, int opt, void *val, int vallen);
    int (*getopt) (int fd, int opt, void *val, int vallen);
};



#endif
