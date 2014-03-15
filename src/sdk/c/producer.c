#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "errno.h"
#include "io.h"
#include "core/rio.h"
#include "os/timestamp.h"

producer_t *producer_new(const char *addr, const char py[PROXYNAME_MAX]) {
    rio_t *io = rio_new();

    rio_init(io);
    if ((io->sockfd = sk_connect("tcp", "", addr)) < 0)
	goto RGS_ERROR;
    sk_setopt(io->sockfd, SK_NONBLOCK, true);
    io->rgh.type = PIO_RCVER;
    uuid_generate(io->rgh.id);
    memcpy(io->rgh.proxyname, py, sizeof(py));
    if (rio_at_rgs(io) < 0 && errno != EAGAIN)
	goto RGS_ERROR;
    while (!bio_empty(&io->out))
	if (bio_flush(&io->out, &io->sock_ops) < 0)
	    goto RGS_ERROR;
    return &io->sockfd;
 RGS_ERROR:
    rio_destroy(io);
    mem_free(io, sizeof(*io));
    return NULL;
}


void producer_destroy(producer_t *pp) {
    rio_t *io = container_of(pp, rio_t, sockfd);
    close(io->sockfd);
    rio_destroy(io);
    mem_free(io, sizeof(*io));
}


int producer_send_request(producer_t *pp, const char *data, uint32_t size) {
    int64_t now = rt_mstime();
    rio_t *io = container_of(pp, rio_t, sockfd);
    struct pio_rt rt = {
	.cost = {0, 0},
	.stay = {0, 0},
	.begin = {0, 0},
    };
    struct pio_hdr h = {
	.version = PIO_VERSION,
	.ttl = 1,
	.end_ttl = 1,
	.size = size,
	.go = 1,
	.flags = 0,
	.seqid = io->seqid++,
	.sendstamp = now,
    };
    uuid_copy(rt.uuid, io->rgh.id);
    ph_makechksum(&h);
    rio_bwrite(io, &h, data, (char *)&rt);
    modstat_update_timestamp(rio_stat(io), now);
    if (rio_flush(io) < 0 && errno != EAGAIN)
	return -1;
    return 0;
}

int producer_psend_request(producer_t *pp, const char *data, uint32_t size) {
    rio_t *io = container_of(pp, rio_t, sockfd);
    if (producer_send_request(pp, data, size) < 0)
	return -1;
    while (rio_flush(io) < 0)
	if (errno != EAGAIN)
	    return -1;
    return 0;
}


int producer_recv_response(producer_t *pp, char **data, uint32_t *size) {
    int64_t now = rt_mstime();
    struct pio_rt *rt = NULL;
    struct pio_hdr h = {};
    rio_t *io = container_of(pp, rio_t, sockfd);

    if ((rio_prefetch(io) < 0 && errno != EAGAIN)
	|| rio_bread(io, &h, data, (char **)&rt) < 0)
	return -1;
    rt->cost[0] = (uint16_t)(now - h.sendstamp - rt->begin[1]);
    pio_rt_print(h.end_ttl, rt);
    mem_free(rt, pio_rt_size(&h));
    if (!ph_validate(&h)) {
	mem_free(data, h.size);
	errno = PIO_ECHKSUM;
	return -1;
    }
    *size = h.size;
    modstat_update_timestamp(rio_stat(io), rt_mstime());
    return 0;
}

int producer_precv_response(producer_t *pp, char **data, uint32_t *size) {
    int ret;
    while ((ret = producer_recv_response(pp, data, size)) < 0 && errno == EAGAIN) {
    }
    return ret;
}
