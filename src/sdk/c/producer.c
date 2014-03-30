#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "io.h"
#include "core/proto_parser.h"


static inline void pio_rt_print(int ttl, pio_rt_t *rt) {
    pio_rt_t *crt;
    crt = rt;
    while (crt < rt + ttl) {
	printf("[go:%d cost:%d stay:%d]%s", crt->begin[0], crt->cost[0],
	       crt->stay[0], (crt == rt + ttl - 1) ? "\n" : " ");
	crt++;
    }
    crt = rt;
    while (crt < rt + ttl) {
	printf("[back:%d cost:%d stay:%d]%s", crt->begin[1], crt->cost[1],
	       crt->stay[1], (crt == rt + ttl - 1) ? "\n" : " ");
	crt++;
    }
}

int producer_send(pio_t *io, const char *data, uint32_t size, int to) {
    int64_t now = rt_mstime();
    proto_parser_t *pp = container_of(io, proto_parser_t, sockfd);
    pio_rt_t rt = {
	.cost = {0, 0},
	.stay = {0, 0},
	.begin = {0, 0},
    };
    pio_hdr_t h = {
	.version = PIO_VERSION,
	.ttl = 1,
	.end_ttl = 1,
	.go = 1,
	.size = size,
	.timeout = (uint16_t)to,
	.sendstamp = now,
	.seqid = pp->seqid++,
    };
    uuid_copy(rt.uuid, pp->rgh.id);
    ph_makechksum(&h);
    proto_parser_bwrite(pp, &h, data, (char *)&rt);
    modstat_update_timestamp(proto_parser_stat(pp), now);
    if (proto_parser_flush(pp) < 0 && errno != EAGAIN)
	return -1;
    return 0;
}

int producer_psend(pio_t *io, const char *data, uint32_t size, int to) {
    proto_parser_t *pp = container_of(io, proto_parser_t, sockfd);
    if (producer_send(io, data, size, to) < 0)
	return -1;
    while (proto_parser_flush(pp) < 0)
	if (errno != EAGAIN)
	    return -1;
    return 0;
}


int producer_recv(pio_t *io, char **data, uint32_t *size) {
    int64_t now = rt_mstime();
    pio_rt_t *rt = NULL;
    pio_hdr_t h = {};
    proto_parser_t *pp = container_of(io, proto_parser_t, sockfd);

    if ((proto_parser_prefetch(pp) < 0 && errno != EAGAIN)
	|| proto_parser_bread(pp, &h, data, (char **)&rt) < 0)
	return -1;
    rt->cost[1] = (uint16_t)(now - h.sendstamp - rt->begin[1]);
    mem_free(rt, rt_size(&h));
    if (!ph_validate(&h)) {
	mem_free(*data, h.size);
	return -1;
    }
    *size = h.size;
    modstat_incrskey(proto_parser_stat(pp), PP_RTT, now - h.sendstamp);
    modstat_update_timestamp(proto_parser_stat(pp), rt_mstime());
    return 0;
}

int producer_precv(pio_t *pp, char **data, uint32_t *size) {
    int rc;
    while ((rc = producer_recv(pp, data, size)) < 0 && errno == EAGAIN) {
    }
    return rc;
}
