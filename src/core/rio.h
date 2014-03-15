#ifndef _HPIO_RIO_
#define _HPIO_RIO_

#include "errno.h"
#include "ds/list.h"
#include "os/memory.h"
#include "os/epoll.h"
#include "bufio/bio.h"
#include "hdr.h"
#include "net/socket.h"
#include "stats/modstat.h"


enum {
    PIO_RECV = 0,
    PIO_SEND,
    PIO_ERROR,
    PIO_RECONNECT,
    PIO_MODSTAT_KEYRANGE,
};
extern const char *pio_modstat_item[PIO_MODSTAT_KEYRANGE];
DEFINE_MODSTAT(pio, PIO_MODSTAT_KEYRANGE);

typedef struct rio {
    int sockfd;
    int64_t seqid;
    pio_modstat_t stat;
    struct bio in;
    struct bio out;
    pio_rgh_t rgh;
    epollevent_t et;
    struct io sock_ops;
    struct list_head io_link;
} rio_t;

#define list_for_each_pio_safe(pos, tmp, head)			\
    list_for_each_entry_safe(pos, tmp, head, rio_t, io_link)

void rio_init(rio_t *io);
void rio_destroy(rio_t *io);

static inline rio_t *rio_new() {
    rio_t *io = (rio_t *)mem_zalloc(sizeof(*io));
    if (io)
	rio_init(io);
    return io;
}

int rio_ps_rgs(rio_t *io);
int rio_at_rgs(rio_t *io);

static inline modstat_t *rio_stat(rio_t *io) {
    return &io->stat.self;
}

int rio_bread(rio_t *io, struct pio_hdr *h, char **data, char **rt);
int rio_bwrite(rio_t *io,
	       const struct pio_hdr *h, const char *data, const char *rt);

static inline int rio_one_ready(rio_t *io) {
    struct pio_hdr h = {};
    struct bio *b = &io->in;
    if ((b->bsize >= sizeof(h)) && ({ bio_copy(b, (char *)&h, sizeof(h));
		b->bsize >= pio_pkg_size(&h);}))
	return true;
    return false;
}

static inline int rio_prefetch(rio_t *io) {
    return bio_prefetch(&io->in, &io->sock_ops);
}

static inline int rio_flush(rio_t *io) {
    while (!bio_empty(&io->out))
	if (bio_flush(&io->out, &io->sock_ops) < 0)
	    return -1;
    return 0;
}


#endif
