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
    char *urt;
    struct pio_hdr h = {};
    proxyio_t *io = container_of(pp, proxyio_t, sockfd);
    struct bio *b = &io->b;

    if (proxyio_recv(io, &h, data, rt) == 0) {
	if (!(urt = mem_realloc(*rt, pio_rt_size(&h) + sizeof(h)))) {
	    mem_free(*data, h.size);
	    mem_free(*rt, pio_rt_size(&h));
	    return -1;
	}
	*size = h.size;
	*rt = urt;
	*rt_size = pio_rt_size(&h) + sizeof(h);
	bio_read(b, urt, sizeof(h));
	bio_read(b, *data, h.size);
	bio_read(b, urt + sizeof(h), pio_rt_size(&h));
	return 0;
    }
    return -1;
}


int comsumer_send_response(comsumer_t *pp, const char *data, int64_t size,
			   const char *urt, int64_t rt_size) {
    proxyio_t *io = container_of(pp, proxyio_t, sockfd);
    struct pio_hdr *h = (struct pio_hdr *)urt;

    h->go = 0;
    h->size = size;
    h->ttl = h->end_ttl = rt_size / PIORTLEN;
    ph_makechksum(h);
    return proxyio_send(io, h, data, urt + sizeof(h));
}

