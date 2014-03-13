#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "errno.h"
#include "proxyio.h"
#include "net/socket.h"

comsumer_t *comsumer_new(const char *addr, const char py[PROXYNAME_MAX]) {
    proxyio_t *io = proxyio_new();

    proxyio_init(io);
    if ((io->sockfd = sk_connect("tcp", "", addr)) < 0)
	goto RGS_ERROR;
    io->rgh.type = PIO_SNDER;
    uuid_generate(io->rgh.id);
    memcpy(io->rgh.proxyname, py, sizeof(py));
    if (proxyio_at_rgs(io) < 0 && errno != EAGAIN)
	goto RGS_ERROR;
    while (!bio_empty(&io->out))
	if (bio_flush(&io->out, &io->sock_ops) < 0)
	    goto RGS_ERROR;
    return &io->sockfd;
 RGS_ERROR:
    proxyio_destroy(io);
    mem_free(io, sizeof(*io));
    return NULL;
}


void comsumer_destroy(comsumer_t *pp) {
    proxyio_t *io = container_of(pp, proxyio_t, sockfd);
    close(io->sockfd);
    proxyio_destroy(io);
    mem_free(io, sizeof(*io));
}



int comsumer_recv_request(comsumer_t *pp, char **data, int64_t *size,
			  char **rt, int64_t *rt_size) {
    char *urt;
    struct pio_hdr h = {};
    proxyio_t *io = container_of(pp, proxyio_t, sockfd);

    if (proxyio_recv(io, &h, data, rt) == 0) {
	if (!(urt = mem_realloc(*rt, pio_rt_size(&h) + sizeof(h)))) {
	    mem_free(*data, h.size);
	    mem_free(*rt, pio_rt_size(&h));
	    return -1;
	}
	*size = h.size;
	*rt = urt;
	*rt_size = pio_rt_size(&h) + sizeof(h);
	memmove((*rt) + sizeof(h), *rt, pio_rt_size(&h));
	memcpy(*rt, (char *)&h, sizeof(h));
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
    proxyio_send(io, h, data, urt + sizeof(h));
    return proxyio_flush(io);
}

