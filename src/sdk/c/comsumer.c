#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "errno.h"
#include "proxyio.h"
#include "net/socket.h"

comsumer_t *comsumer_new(const char *addr, const char grp[GRPNAME_MAX]) {
    proxyio_t *io = proxyio_new();

    bio_init(&io->b);
    if (proxyio_register(io, addr, grp, PIO_SNDER) < 0) {
	mem_free(io, sizeof(*io));
	return NULL;
    }
    return &io->sockfd;
}


void comsumer_destroy(comsumer_t *pp) {
    proxyio_t *io = container_of(pp, proxyio_t, sockfd);
    bio_destroy(&io->b);
    close(io->sockfd);
    mem_free(io, sizeof(*io));
}



int comsumer_recv_request(comsumer_t *pp, char **data, int64_t *size,
			  char **rt, int64_t *rt_size) {
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
    if (!(*rt = (char *)mem_zalloc(sizeof(h) + pio_rt_size(&h)))) {
	mem_free(*data, h.size);
	*data = NULL;
	return -1;
    }
    *size = h.size;
    *rt_size = pio_rt_size(&h);
    bio_read(b, *rt, sizeof(h));
    bio_read(b, *data, h.size);
    bio_read(b, (*rt) + sizeof(h), *rt_size);
    return 0;
}


int comsumer_send_response(comsumer_t *pp, const char *data, int64_t size,
			   const char *_rt, int64_t rt_size) {
    proxyio_t *io = container_of(pp, proxyio_t, sockfd);
    struct pio_hdr *h = (struct pio_hdr *)_rt;
    struct pio_rt *rt = (struct pio_rt *)(_rt + sizeof(h));

    if (ph_validate(h)) {
	errno = PIO_ECHKSUM;
	return -1;
    }
    h->go = 0;
    h->size = size;
    ph_makechksum(h);
    BUG_ON(bio_write(&io->b, (char *)h, sizeof(h)) != sizeof(h));
    BUG_ON(bio_write(&io->b, data, size) != size);
    BUG_ON(bio_write(&io->b, (char *)rt, pio_rt_size(h)) != pio_rt_size(h));
    while (!bio_empty(&io->b))
	if (bio_flush(&io->b, &io->sock_ops) < 0 && errno != EAGAIN)
	    return -1;
    return 0;
}

