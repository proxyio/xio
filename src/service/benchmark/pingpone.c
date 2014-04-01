#include <stdio.h>
#include "bc.h"
#include "opt.h"
#include "os/eventloop.h"
#include "sdk/c/io.h"

#define REQLEN PAGE_SIZE
static char page[REQLEN];
extern int randstr(char *buff, int sz);

static inline int producer_event_handler(eloop_t *, ev_t *);
static inline int comsumer_event_handler(eloop_t *, ev_t *);

static proto_parser_t *new_pingpong_producer(pingpong_ctx_t *ctx) {
    pio_t *io;
    proto_parser_t *pp;
    pmsg_t *msg = alloc_pio_msg(1);
    struct bc_opt *cf = ctx->cf;
    
    if (!(io = pio_join_producer(cf->host, proxyname)))
	return NULL;
    pp = container_of(io, proto_parser_t, sockfd);
    pp->et.fd = pp->sockfd;
    pp->et.events = EPOLLIN;
    pp->et.f = producer_event_handler;
    if (eloop_add(&ctx->el, &pp->et) < 0) {
	pio_close(io);
	return NULL;
    }
    modstat_set_warnf(proto_parser_stat(pp), MSL_S, bc_threshold_warn);
    list_add(&pp->pp_link, &ctx->pp_head);
    msg->vec[0].iov_len = cf->size > 0 ? cf->size : rand() % REQLEN;
    msg->vec[0].iov_base = page;
    BUG_ON(pio_sendmsg(&pp->sockfd, msg) != 0);
    free_pio_msg_and_vec(msg);
    return pp;
}

static proto_parser_t *new_pingpong_comsumer(pingpong_ctx_t *ctx) {
    pio_t *io;
    proto_parser_t *pp;
    struct bc_opt *cf = ctx->cf;
    
    if (!(io = pio_join_comsumer(cf->host, proxyname)))
	return NULL;
    pp = container_of(io, proto_parser_t, sockfd);
    pp->et.fd = pp->sockfd;
    pp->et.events = EPOLLIN;
    pp->et.f = comsumer_event_handler;
    if (eloop_add(&ctx->el, &pp->et) < 0) {
	pio_close(io);
	return NULL;
    }
    modstat_set_warnf(proto_parser_stat(pp), MSL_S, bc_threshold_warn);
    list_add(&pp->pp_link, &ctx->pp_head);
    return pp;
}


static inline int
producer_event_handler(eloop_t *el, ev_t *et) {
    proto_parser_t *pp = container_of(et, proto_parser_t, et);
    pingpong_ctx_t *ctx = container_of(el, pingpong_ctx_t, el);
    pmsg_t *msg;

    if (et->happened & (EPOLLERR|EPOLLRDHUP)) {
	eloop_del(el, et);
	list_del(&pp->pp_link);
	pio_close(&pp->sockfd);
	while (!new_pingpong_producer(ctx))
	    usleep(1000);
	return 0;
    }
    if (pio_recvmsg(&pp->sockfd, &msg) == 0) {
	pio_sendmsg(&pp->sockfd, msg);
	free_pio_msg_and_vec(msg);
    }
    return 0;
}

static inline int
comsumer_event_handler(eloop_t *el, ev_t *et) {
    proto_parser_t *pp = container_of(et, proto_parser_t, et);
    pingpong_ctx_t *ctx = container_of(el, pingpong_ctx_t, el);
    pmsg_t *msg;

    if (et->happened & (EPOLLERR|EPOLLRDHUP)) {
	eloop_del(el, et);
	list_del(&pp->pp_link);
	pio_close(&pp->sockfd);
	while (!new_pingpong_comsumer(ctx))
	    usleep(1000);
	return 0;
    }
    if (pio_recvmsg(&pp->sockfd, &msg) == 0) {
	pio_sendmsg(&pp->sockfd, msg);
	free_pio_msg_and_vec(msg);
    }
    return 0;
}


int pingpong_start(struct bc_opt *cf) {
    int i;
    proto_parser_t *pp, *tmp;
    pingpong_ctx_t ctx = {};

    randstr(page, REQLEN);
    INIT_LIST_HEAD(&ctx.pp_head);
    ctx.cf = cf;

    eloop_init(&ctx.el, 10240, 100, 1);
    for (i = 0; i < cf->comsumer_num; i++)
	while (!new_pingpong_comsumer(&ctx))
	    usleep(1000);
    for (i = 0; i < cf->producer_num; i++)
	while (!new_pingpong_producer(&ctx))
	    usleep(1000);
    while (rt_mstime() < cf->deadline)
	eloop_once(&ctx.el);
    list_for_each_pio_safe(pp, tmp, &ctx.pp_head) {
	eloop_del(&ctx.el, &pp->et);
	list_del(&pp->pp_link);
	pio_close(&pp->sockfd);
    }
    eloop_destroy(&ctx.el);
    return 0;
}
