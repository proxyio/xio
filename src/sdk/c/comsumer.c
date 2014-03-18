#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "io.h"
#include "core/proto_parser.h"
#include "net/socket.h"

int comsumer_send(pio_t *io, const char *data, uint32_t size, const char *urt, uint32_t rt_sz) {
    int64_t now = rt_mstime();
    proto_parser_t *pp = container_of(io, proto_parser_t, sockfd);
    pio_rt_t *crt;
    pio_hdr_t *h = (pio_hdr_t *)urt;

    if (!ph_validate(h))
	return -1;
    h->go = 0;
    h->size = size;
    h->end_ttl = h->ttl;
    crt = &((pio_rt_t *)(urt + PIOHDRLEN))[h->ttl - 1];
    crt->begin[1] = (uint16_t)(now - h->sendstamp);
    crt->stay[0] = (uint16_t)(now - h->sendstamp - crt->begin[0] - crt->cost[0]);
    ph_makechksum(h);
    proto_parser_bwrite(pp, h, data, urt + PIOHDRLEN);
    modstat_update_timestamp(proto_parser_stat(pp), now);
    if (proto_parser_flush(pp) < 0 && errno != EAGAIN)
	return -1;
    return 0;
}


int comsumer_psend(pio_t *io, const char *data, uint32_t size, const char *urt, uint32_t rt_sz) {
    proto_parser_t *pp = container_of(io, proto_parser_t, sockfd);
    if (comsumer_send(io, data, size, urt, rt_sz) < 0)
	return -1;
    while (proto_parser_flush(pp) < 0)
	if (errno != EAGAIN)
	    return -1;
    return 0;
}

int comsumer_recv(pio_t *io, char **data, uint32_t *size, char **rt, uint32_t *rt_sz) {
    int64_t now = rt_mstime();
    char *urt;
    pio_rt_t *crt;
    pio_hdr_t h = {};
    proto_parser_t *pp = container_of(io, proto_parser_t, sockfd);

    if ((proto_parser_prefetch(pp) < 0 && errno != EAGAIN) || (proto_parser_bread(pp, &h, data, rt) < 0))
	return -1;
    if (!ph_validate(&h)) {
	mem_free(*data, h.size);
	mem_free(*rt, rt_size(&h));
	return -1;
    }
    if (!(urt = mem_realloc(*rt, rt_size(&h) + PIOHDRLEN))) {
	mem_free(*data, h.size);
	mem_free(*rt, rt_size(&h));
	return -1;
    }
    crt = &((pio_rt_t *)urt)[h.ttl - 1];
    crt->cost[0] = (uint16_t)(now - h.sendstamp - crt->begin[0]);
    *size = h.size;
    *rt = urt;
    *rt_sz = rt_size(&h) + PIOHDRLEN;
    memmove((*rt) + PIOHDRLEN, *rt, rt_size(&h));
    memcpy(*rt, (char *)&h, PIOHDRLEN);
    modstat_incrskey(proto_parser_stat(pp), PP_RTT, now - h.sendstamp);
    modstat_update_timestamp(proto_parser_stat(pp), now);
    return 0;
}

int comsumer_precv(pio_t *pp, char **data, uint32_t *size, char **rt, uint32_t *rt_sz) {
    int ret;
    while ((ret = comsumer_recv(pp, data, size, rt, rt_sz)) < 0
	   && errno == EAGAIN) {
    }
    return ret;
}

