#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "rio.h"
#include "net/socket.h"

const char *pio_modstat_item[PIO_MODSTAT_KEYRANGE] = {
    "RECV",
    "SEND",
    "ERROR",
    "RECONNECT",
};

static inline int64_t rio_sock_read(struct io *sock, char *buff, int64_t sz) {
    rio_t *io = container_of(sock, rio_t, sock_ops);
    sz = sk_read(io->sockfd, buff, sz);
    return sz;
}

static inline int64_t rio_sock_write(struct io *sock, char *buff, int64_t sz) {
    rio_t *io = container_of(sock, rio_t, sock_ops);
    sz = sk_write(io->sockfd, buff, sz);
    return sz;
}


void rio_init(rio_t *io) {
    ZERO(*io);
    INIT_MODSTAT(io->stat);
    bio_init(&io->in);
    bio_init(&io->out);
    io->sock_ops.read = rio_sock_read;
    io->sock_ops.write = rio_sock_write;
}

void rio_destroy(rio_t *io) {
    bio_destroy(&io->in);
    bio_destroy(&io->out);
}


int rio_ps_rgs(rio_t *io) {
    struct bio *b = &io->in;
    modstat_t *stat = rio_stat(io);

    while ((b->bsize < sizeof(io->rgh)))
	if (bio_prefetch(b, &io->sock_ops) < 0)
	    return -1;
    modstat_incrkey(stat, PIO_RECONNECT);
    bio_read(b, (char *)&io->rgh, sizeof(io->rgh));
    return 0;
}

int rio_at_rgs(rio_t *io) {
    struct bio *b = &io->out;
    modstat_t *stat = rio_stat(io);

    bio_write(b, (char *)&io->rgh, sizeof(io->rgh));
    bio_flush(b, &io->sock_ops);
    modstat_incrkey(stat, PIO_RECONNECT);
    return 0;
}


int rio_bread(rio_t *io,
	      struct pio_hdr *h, char **data, char **rt) {
    struct bio *b = &io->in;
    modstat_t *stat = rio_stat(io);

    if (!rio_one_ready(io)) {
	errno = EAGAIN;
	return -1;
    }
    modstat_incrkey(stat, PIO_RECV);
    bio_copy(b, (char *)h, sizeof(*h));
    if (!(*data = (char *)mem_zalloc(h->size)))
	return -1;
    if (!(*rt = (char *)mem_zalloc(pio_rt_size(h)))) {
	mem_free(*data, h->size);
	*data = NULL;
	return -1;
    }
    bio_read(b, (char *)h, sizeof(*h));
    bio_read(b, *data, h->size);
    bio_read(b, *rt, pio_rt_size(h));
    return 0;
}


int rio_bwrite(rio_t *io,
	       const struct pio_hdr *h, const char *data, const char *rt) {
    modstat_t *stat = rio_stat(io);

    modstat_incrkey(stat, PIO_SEND);
    bio_write(&io->out, (char *)h, sizeof(*h));
    bio_write(&io->out, data, h->size);
    bio_write(&io->out, rt, pio_rt_size(h));
    return 0;
}

