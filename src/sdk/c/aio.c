#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "aio.h"
#include "core/proto_parser.h"
#include "runner/taskpool.h"

struct apio {
    apio_cf_t cf;
    pio_t *io;
    epoll_t el;
    taskpool_t tp;
    request_come_f qf;
    response_back_f pf;
};

static inline apio_t *apio_new() {
    apio_t *aio = (apio_t *)mem_zalloc(sizeof(*aio));
    return aio;
}

typedef struct kreplyer {
    apio_t *aio;
    char *rt;
    uint32_t rt_sz;
    replyer_t ry;
} kreplyer_t;


apio_t *apio_join_comsumer(apio_cf_t *cf, request_come_f f) {
    apio_t *aio;

    if (!(aio = apio_new()))
	return NULL;
    if (!(aio->io = pio_join_comsumer(cf->host, cf->proxy))) {
	mem_free(aio, sizeof(*aio));
	return NULL;
    }
    aio->cf = *cf;
    aio->qf = f;
    epoll_init(&aio->el, 10240, 100, 1);
    taskpool_init(&aio->tp, cf->max_cpus);
    return aio;
}


apio_t *apio_join_producer(apio_cf_t *cf, response_back_f f) {
    apio_t *aio;

    if (!(aio = apio_new()))
	return NULL;
    if (!(aio->io = pio_join_producer(cf->host, cf->proxy))) {
	mem_free(aio, sizeof(*aio));
	return NULL;
    }
    aio->cf = *cf;
    aio->pf = f;
    epoll_init(&aio->el, 10240, 100, 1);
    taskpool_init(&aio->tp, cf->max_cpus);
    return aio;
}


int apio_reactor(void *args) {
    epoll_t *el = (epoll_t *)args;
    return epoll_startloop(el);
}

static int producer_event_handler(epoll_t *el, epollevent_t *et) {
    return 0;
}

int apio_start(apio_t *aio) {
    proto_parser_t *pp = container_of(aio->io, proto_parser_t, sockfd);
    epollevent_t *et = &pp->et;

    et->fd = *aio->io;
    et->f = producer_event_handler;
    et->events = EPOLLIN|EPOLLOUT;
    return 0;
}

