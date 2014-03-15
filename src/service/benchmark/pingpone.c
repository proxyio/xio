#include <stdio.h>
#include "bc.h"
#include "opt.h"
#include "os/epoll.h"
#include "os/timestamp.h"
#include "core/rio.h"
#include "sdk/c/io.h"

#define REQLEN PAGE_SIZE
extern int randstr(char *buff, int sz);

static inline int producer_event_handler(epoll_t *, epollevent_t *, uint32_t);
static inline int comsumer_event_handler(epoll_t *, epollevent_t *, uint32_t);

static rio_t *new_pingpong_producer(pingpong_ctx_t *ctx) {
    int *sockfd;
    rio_t *io;
    struct bc_opt *cf = ctx->cf;
    
    if (!(sockfd = producer_new(cf->host, cf->proxyname)))
	return NULL;
    io = container_of(sockfd, rio_t, sockfd);
    io->et.fd = *sockfd;
    io->et.events = EPOLLIN;
    io->et.f = producer_event_handler;
    if (epoll_add(&ctx->el, &io->et) < 0) {
	producer_destroy(sockfd);
	return NULL;
    }
    modstat_set_warnf(rio_stat(io), MSL_S, bc_threshold_warn);
    list_add(&io->io_link, &ctx->io_head);
    return io;
}

static rio_t *new_pingpong_comsumer(pingpong_ctx_t *ctx) {
    int *sockfd;
    rio_t *io;
    struct bc_opt *cf = ctx->cf;
    
    if (!(sockfd = comsumer_new(cf->host, cf->proxyname)))
	return NULL;
    io = container_of(sockfd, rio_t, sockfd);
    io->et.fd = *sockfd;
    io->et.events = EPOLLIN;
    io->et.f = comsumer_event_handler;
    if (epoll_add(&ctx->el, &io->et) < 0) {
	comsumer_destroy(sockfd);
	return NULL;
    }
    modstat_set_warnf(rio_stat(io), MSL_S, bc_threshold_warn);
    list_add(&io->io_link, &ctx->io_head);
    return io;
}


static inline int
producer_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    rio_t *io = container_of(et, rio_t, et);
    pingpong_ctx_t *ctx = container_of(el, pingpong_ctx_t, el);
    char *data;
    uint32_t sz;
    if (happened & EPOLLERR) {
	epoll_del(el, et);
	rio_destroy(io);
	new_pingpong_producer(ctx);
	return 0;
    }
    if (producer_recv_response(&io->sockfd, &data, &sz) == 0) {
	producer_psend_request(&io->sockfd, data, sz);
	mem_free(data, sz);
    }
    return 0;
}

static inline int
comsumer_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    rio_t *io = container_of(et, rio_t, et);
    pingpong_ctx_t *ctx = container_of(el, pingpong_ctx_t, el);
    char *data, *rt;
    uint32_t sz, rt_sz;
    if (happened & EPOLLERR) {
	epoll_del(el, et);
	rio_destroy(io);
	new_pingpong_comsumer(ctx);
	return 0;
    }
    if (comsumer_recv_request(&io->sockfd, &data, &sz, &rt, &rt_sz) == 0) {
	comsumer_psend_response(&io->sockfd, data, sz, rt, rt_sz);
	mem_free(data, sz);
	mem_free(rt, rt_sz);
    }
    return 0;
}


int pingpong_start(struct bc_opt *cf) {
    int i;
    uint32_t sz;
    char page[REQLEN];
    rio_t *io, *tmp;
    pingpong_ctx_t ctx = {};

    INIT_LIST_HEAD(&ctx.io_head);
    ctx.cf = cf;
    randstr(page, REQLEN);
    epoll_init(&ctx.el, 10240, 100, 1);
    for (i = 0; i < cf->comsumer_num; i++)
	new_pingpong_comsumer(&ctx);
    for (i = 0; i < cf->producer_num; i++) {
	io = new_pingpong_producer(&ctx);
	sz = cf->size > 0 ? cf->size : rand() % REQLEN;
	BUG_ON(producer_psend_request(&io->sockfd, page, sz) != 0);
    }
    while (rt_mstime() < cf->deadline)
	epoll_oneloop(&ctx.el);
    list_for_each_pio_safe(io, tmp, &ctx.io_head) {
	epoll_del(&ctx.el, &io->et);
	list_del(&io->io_link);
	rio_destroy(io);
    }
    epoll_destroy(&ctx.el);
    return 0;
}
