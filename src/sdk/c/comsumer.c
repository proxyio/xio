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
    mem_free(io, sizeof(*io));
}



int comsumer_recv_request(comsumer_t *pp, char **data, int64_t *size,
			  char **rt, int64_t *rt_size) {
    int ret, hready = false;
    struct pio_hdr h = {};    
    proxyio_t *io = container_of(pp, proxyio_t, sockfd);
    struct bio *b = &io->b;
    
    do {
	ret = bio_prefetch(b, &io->sock_ops);
	if (!hready && b->bsize >= sizeof(h)) {
	    hready = true;
	    bio_copy(b, (char *)&h, sizeof(h));
	    if (!ph_validate(&h)) {
		errno = PIO_ECHKSUM;
		return -1;
	    }
	}
    } while (ret < 0 && errno != EAGAIN &&
	     (b->bsize < sizeof(h) || b->bsize < pio_pkg_size(&h)));
    if (hready && b->bsize >= pio_pkg_size(&h)) {
	if (!(*data = (char *)mem_zalloc(h.size)))
	    return -1;
	if (!(*rt = (char *)mem_zalloc(sizeof(h) + pio_rt_size(&h)))) {
	    *data = NULL;
	    mem_free(*data, h.size);
	    return -1;
	}
	*size = h.size;
	*rt_size = pio_rt_size(&h);
	bio_read(b, *rt, sizeof(h));
	bio_read(b, *data, h.size);
	bio_read(b, (*rt) + sizeof(h), *rt_size);
	return 0;
    }
    return ret >= 0 ? 0 : -1;
}


int comsumer_send_response(comsumer_t *pp, const char *data, int64_t size,
			   const char *_rt, int64_t rt_size) {
    int ret;
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
    while (!bio_empty(&io->b)) {
	if ((ret = bio_flush(&io->b, &io->sock_ops)) < 0 && errno != EAGAIN)
	    break;
    }
    return ret >= 0 ? 0 : -1;
}

