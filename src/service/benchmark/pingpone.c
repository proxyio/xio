#include <stdio.h>
#include "bc.h"
#include "opt.h"
#include "os/epoll.h"
#include "sdk/c/io.h"

#define REQLEN PAGE_SIZE
extern int randstr(char *buff, int sz);

static inline int producer_event_handler(epoll_t *, epollevent_t *, uint32_t);
static inline int comsumer_event_handler(epoll_t *, epollevent_t *, uint32_t);

static proto_parser_t *new_pingpong_producer(pingpong_ctx_t *ctx) {
    uint32_t sz;
    pio_t *io;
    proto_parser_t *pp;
    char page[REQLEN];
    struct bc_opt *cf = ctx->cf;
    
    randstr(page, REQLEN);
    if (!(io = pio_join(cf->host, cf->proxyname, PRODUCER)))
	return NULL;
    pp = container_of(io, proto_parser_t, sockfd);
    pp->et.fd = pp->sockfd;
    pp->et.events = EPOLLIN;
    pp->et.f = producer_event_handler;
    if (epoll_add(&ctx->el, &pp->et) < 0) {
	pio_close(io);
	return NULL;
    }
    modstat_set_warnf(proto_parser_stat(pp), MSL_S, bc_threshold_warn);
    list_add(&pp->pp_link, &ctx->pp_head);
    sz = cf->size > 0 ? cf->size : rand() % REQLEN;
    BUG_ON(producer_psend_request(&pp->sockfd, page, sz, cf->timeout) != 0);
    return pp;
}

static proto_parser_t *new_pingpong_comsumer(pingpong_ctx_t *ctx) {
    pio_t *io;
    proto_parser_t *pp;
    struct bc_opt *cf = ctx->cf;
    
    if (!(io = pio_join(cf->host, cf->proxyname, COMSUMER)))
	return NULL;
    pp = container_of(io, proto_parser_t, sockfd);
    pp->et.fd = pp->sockfd;
    pp->et.events = EPOLLIN;
    pp->et.f = comsumer_event_handler;
    if (epoll_add(&ctx->el, &pp->et) < 0) {
	pio_close(io);
	return NULL;
    }
    modstat_set_warnf(proto_parser_stat(pp), MSL_S, bc_threshold_warn);
    list_add(&pp->pp_link, &ctx->pp_head);
    return pp;
}


static inline int
producer_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    proto_parser_t *pp = container_of(et, proto_parser_t, et);
    pingpong_ctx_t *ctx = container_of(el, pingpong_ctx_t, el);
    struct bc_opt *cf = ctx->cf;
    char *data;
    uint32_t sz;
    if (happened & (EPOLLERR|EPOLLRDHUP)) {
	epoll_del(el, et);
	list_del(&pp->pp_link);
	pio_close(&pp->sockfd);
	while (!new_pingpong_producer(ctx))
	    usleep(1000);
	return 0;
    }
    if (producer_recv_response(&pp->sockfd, &data, &sz) == 0) {
	producer_psend_request(&pp->sockfd, data, sz, cf->timeout);
	mem_free(data, sz);
    }
    return 0;
}

static inline int
comsumer_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    proto_parser_t *pp = container_of(et, proto_parser_t, et);
    pingpong_ctx_t *ctx = container_of(el, pingpong_ctx_t, el);
    char *data, *rt;
    uint32_t sz, rt_sz;
    if (happened & (EPOLLERR|EPOLLRDHUP)) {
	epoll_del(el, et);
	list_del(&pp->pp_link);
	pio_close(&pp->sockfd);
	while (!new_pingpong_comsumer(ctx))
	    usleep(1000);
	return 0;
    }
    if (comsumer_recv_request(&pp->sockfd, &data, &sz, &rt, &rt_sz) == 0) {
	comsumer_psend_response(&pp->sockfd, data, sz, rt, rt_sz);
	mem_free(data, sz);
	mem_free(rt, rt_sz);
    }
    return 0;
}


int pingpong_start(struct bc_opt *cf) {
    int i;
    proto_parser_t *pp, *tmp;
    pingpong_ctx_t ctx = {};

    INIT_LIST_HEAD(&ctx.pp_head);
    ctx.cf = cf;

    epoll_init(&ctx.el, 10240, 100, 1);
    for (i = 0; i < cf->comsumer_num; i++)
	while (!new_pingpong_comsumer(&ctx))
	    usleep(1000);
    for (i = 0; i < cf->producer_num; i++)
	while (!new_pingpong_producer(&ctx))
	    usleep(1000);
    while (rt_mstime() < cf->deadline)
	epoll_oneloop(&ctx.el);
    list_for_each_pio_safe(pp, tmp, &ctx.pp_head) {
	epoll_del(&ctx.el, &pp->et);
	list_del(&pp->pp_link);
	pio_close(&pp->sockfd);
    }
    epoll_destroy(&ctx.el);
    return 0;
}
