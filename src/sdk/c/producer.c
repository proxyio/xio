#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "errno.h"
#include "proxyio.h"
#include "os/timestamp.h"

producer_t *producer_new(const char *addr, const char py[PROXYNAME_MAX]) {
    proxyio_t *io = proxyio_new();

    proxyio_init(io);
    if ((io->sockfd = sk_connect("tcp", "", addr)) < 0)
	goto RGS_ERROR;
    io->rgh.type = PIO_RCVER;
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


void producer_destroy(producer_t *pp) {
    proxyio_t *io = container_of(pp, proxyio_t, sockfd);
    close(io->sockfd);
    proxyio_destroy(io);
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
    proxyio_send(io, &h, data, (char *)&rt);
    return proxyio_flush(io);
}


int producer_recv_response(producer_t *pp, char **data, int64_t *size) {
    struct pio_rt *rt = NULL;
    struct pio_hdr h = {};
    proxyio_t *io = container_of(pp, proxyio_t, sockfd);

    if (proxyio_recv(io, &h, data, (char **)&rt) == 0) {
	*size = h.size;
	mem_free(rt, sizeof(*rt));
	return 0;
    }
    return -1;
}