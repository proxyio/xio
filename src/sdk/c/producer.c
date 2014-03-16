#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "io.h"
#include "core/proto_parser.h"

producer_t *producer_new(const char *addr, const char py[PROXYNAME_MAX]) {
    proto_parser_t *pp = proto_parser_new();

    proto_parser_init(pp);
    if ((pp->sockfd = sk_connect("tcp", "", addr)) < 0)
	goto RGS_ERROR;
    sk_setopt(pp->sockfd, SK_NONBLOCK, true);
    pp->rgh.type = PIO_RCVER;
    uuid_generate(pp->rgh.id);
    memcpy(pp->rgh.proxyname, py, sizeof(py));
    if (proto_parser_at_rgs(pp) < 0 && errno != EAGAIN)
	goto RGS_ERROR;
    while (!bio_empty(&pp->out))
	if (bio_flush(&pp->out, &pp->sock_ops) < 0)
	    goto RGS_ERROR;
    return &pp->sockfd;
 RGS_ERROR:
    proto_parser_destroy(pp);
    mem_free(pp, sizeof(*pp));
    return NULL;
}


void producer_destroy(producer_t *io) {
    proto_parser_t *pp = container_of(io, proto_parser_t, sockfd);
    close(pp->sockfd);
    proto_parser_destroy(pp);
    mem_free(pp, sizeof(*pp));
}


int producer_send_request(producer_t *io, const char *data, uint32_t size) {
    int64_t now = rt_mstime();
    proto_parser_t *pp = container_of(io, proto_parser_t, sockfd);
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
	.seqid = pp->seqid++,
	.sendstamp = now,
    };
    uuid_copy(rt.uuid, pp->rgh.id);
    ph_makechksum(&h);
    proto_parser_bwrite(pp, &h, data, (char *)&rt);
    modstat_update_timestamp(proto_parser_stat(pp), now);
    if (proto_parser_flush(pp) < 0 && errno != EAGAIN)
	return -1;
    return 0;
}

int producer_psend_request(producer_t *io, const char *data, uint32_t size) {
    proto_parser_t *pp = container_of(io, proto_parser_t, sockfd);
    if (producer_send_request(io, data, size) < 0)
	return -1;
    while (proto_parser_flush(pp) < 0)
	if (errno != EAGAIN)
	    return -1;
    return 0;
}


int producer_recv_response(producer_t *io, char **data, uint32_t *size) {
    int64_t now = rt_mstime();
    struct pio_rt *rt = NULL;
    struct pio_hdr h = {};
    proto_parser_t *pp = container_of(io, proto_parser_t, sockfd);

    if ((proto_parser_prefetch(pp) < 0 && errno != EAGAIN)
	|| proto_parser_bread(pp, &h, data, (char **)&rt) < 0)
	return -1;
    rt->cost[1] = (uint16_t)(now - h.sendstamp - rt->begin[1]);
    mem_free(rt, pio_rt_size(&h));
    if (!ph_validate(&h)) {
	mem_free(data, h.size);
	return -1;
    }
    *size = h.size;
    modstat_incrskey(proto_parser_stat(pp), PP_RTT, now - h.sendstamp);
    modstat_update_timestamp(proto_parser_stat(pp), rt_mstime());
    return 0;
}

int producer_precv_response(producer_t *pp, char **data, uint32_t *size) {
    int ret;
    while ((ret = producer_recv_response(pp, data, size)) < 0 && errno == EAGAIN) {
    }
    return ret;
}
