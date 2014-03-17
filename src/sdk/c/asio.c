#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "asio.h"
#include "core/proto_parser.h"
#include "net/socket.h"
#include "runner/taskpool.h"

struct aspio {
    aspio_cf_t cf;
    pio_t *io;
    epoll_t el;
    taskpool_t tp;
    request_come_f qf;
    response_back_f pf;
};

static inline aspio_t *aspio_new() {
    aspio_t *asio = (aspio_t *)mem_zalloc(sizeof(*asio));
    return asio;
}

typedef struct kresp_writer {
    aspio_t *asio;
    char *rt;
    uint32_t rt_sz;
    resp_writer_t rw;
} kresp_writer_t;


aspio_t *aspio_join_comsumer(aspio_cf_t *cf, request_come_f f) {
    aspio_t *asio;

    if (!(asio = aspio_new()))
	return NULL;
    if (!(asio->io = pio_join(cf->host, cf->proxy, COMSUMER))) {
	mem_free(asio, sizeof(*asio));
	return NULL;
    }
    asio->cf = *cf;
    asio->qf = f;
    epoll_init(&asio->el, 10240, 100, 1);
    taskpool_init(&asio->tp, cf->max_cpus);
    return asio;
}


aspio_t *aspio_join_producer(aspio_cf_t *cf, response_back_f f) {
    aspio_t *asio;

    if (!(asio = aspio_new()))
	return NULL;
    if (!(asio->io = pio_join(cf->host, cf->proxy, PRODUCER))) {
	mem_free(asio, sizeof(*asio));
	return NULL;
    }
    asio->cf = *cf;
    asio->pf = f;
    epoll_init(&asio->el, 10240, 100, 1);
    taskpool_init(&asio->tp, cf->max_cpus);
    return asio;
}


int aspio_reactor(void *args) {
    epoll_t *el = (epoll_t *)args;
    return epoll_startloop(el);
}

static int producer_event_handler(epoll_t *el, epollevent_t *et) {
    return 0;
}

int aspio_start(aspio_t *asio) {
    proto_parser_t *pp = container_of(asio->io, proto_parser_t, sockfd);
    epollevent_t *et = &pp->et;

    et->fd = *asio->io;
    et->f = producer_event_handler;
    et->events = EPOLLIN|EPOLLOUT;
    return 0;
}

