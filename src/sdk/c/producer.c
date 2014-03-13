#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "errno.h"
#include "proxyio.h"
#include "os/timestamp.h"

producer_t *producer_new(const char *addr, const char grp[GRPNAME_MAX]) {
    proxyio_t *io = proxyio_new();

    bio_init(&io->b);
    if (proxyio_register(io, addr, grp, PIO_RCVER) < 0) {
	mem_free(io, sizeof(*io));
	return NULL;
    }
    return &io->sockfd;
}


void producer_destroy(producer_t *pp) {
    proxyio_t *io = container_of(pp, proxyio_t, sockfd);
    bio_destroy(&io->b);
    close(io->sockfd);
    mem_free(io, sizeof(*io));
}


int producer_send_request(producer_t *pp, const char *data, int64_t size) {
    proxyio_t *io = container_of(pp, proxyio_t, sockfd);
    struct pio_rt rt = {
	.cost = 0,
	.stay = 0,
	.begin = 0,
    };
    struct pio_hdr h = {
	.version = PIO_VERSION,
	.ttl = 1,
	.end_ttl = 0,
	.size = size,
	.go = 1,
	.flags = 0,
	.seqid = io->seqid++,
	.sendstamp = rt_mstime(),
    };
    uuid_copy(rt.uuid, io->rgh.id);
    ph_makechksum(&h);
    BUG_ON(bio_write(&io->b, (char *)&h, sizeof(h)) != sizeof(h));
    BUG_ON(bio_write(&io->b, data, size) != size);
    BUG_ON(bio_write(&io->b, (char *)&rt, sizeof(rt)) != sizeof(rt));
    while (!bio_empty(&io->b))
	if (bio_flush(&io->b, &io->sock_ops) < 0 && errno != EAGAIN)
	    return -1;
    return 0;
}


int producer_recv_response(producer_t *pp, char **data, int64_t *size) {
    struct pio_rt rt;
    struct pio_hdr h = {};    
    proxyio_t *io = container_of(pp, proxyio_t, sockfd);
    struct bio *b = &io->b;

    if ((b->bsize >= sizeof(h)) && !({
		bio_copy(b, (char *)&h, sizeof(h)); ph_validate(&h);})) {
	errno = PIO_ECHKSUM;
	return -1;
    }
    while (b->bsize < sizeof(h) || b->bsize < pio_pkg_size(&h))
	if (bio_prefetch(b, &io->sock_ops) < 0 && errno != EAGAIN)
	    return -1;
    if (!(*data = (char *)mem_zalloc(h.size)))
	return -1;
    *size = h.size;
    bio_read(b, (char *)&h, sizeof(h));
    bio_read(b, *data, h.size);
    bio_read(b, (char *)&rt, sizeof(rt));
    return 0;
}
