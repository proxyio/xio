#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "errno.h"
#include "proxyio.h"
#include "net/socket.h"


int proxyio_ps_rgs(proxyio_t *io) {
    struct bio *b = &io->in;

    while ((b->bsize < sizeof(io->rgh)))
	if (bio_prefetch(b, &io->sock_ops) < 0)
	    return -1;
    bio_read(b, (char *)&io->rgh, sizeof(io->rgh));
    return 0;
}

int proxyio_at_rgs(proxyio_t *io) {
    struct bio *b = &io->out;

    bio_write(b, (char *)&io->rgh, sizeof(io->rgh));
    bio_flush(b, &io->sock_ops);
    return 0;
}


int proxyio_recv(proxyio_t *io,
		 struct pio_hdr *h, char **data, char **rt) {
    struct bio *b = &io->in;

    if (!proxyio_one_ready(io)) {
	errno = EAGAIN;
	return -1;
    }
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


int proxyio_send(proxyio_t *io,
		 const struct pio_hdr *h, const char *data, const char *rt) {
    bio_write(&io->out, (char *)h, sizeof(*h));
    bio_write(&io->out, data, h->size);
    bio_write(&io->out, rt, pio_rt_size(h));
    return 0;
}

