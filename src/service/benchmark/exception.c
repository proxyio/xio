#include <stdio.h>
#include "bc.h"
#include "opt.h"
#include "os/epoll.h"
#include "os/timestamp.h"
#include "sdk/c/proxyio.h"

#define REQLEN (PAGE_SIZE * 8)
static char page[REQLEN];
extern int randstr(char *buff, int sz);

static inline int producer_event_handler(epoll_t *, epollevent_t *, uint32_t);
static inline int comsumer_event_handler(epoll_t *, epollevent_t *, uint32_t);

static proxyio_t *new_pingpong_producer(pingpong_ctx_t *ctx) {
    int *sockfd;
    proxyio_t *io;
    struct bc_opt *cf = ctx->cf;
    
    if (!(sockfd = producer_new(cf->host, cf->proxyname)))
	return NULL;
    io = container_of(sockfd, proxyio_t, sockfd);
    io->et.fd = *sockfd;
    io->et.events = EPOLLIN|EPOLLOUT;
    io->et.f = comsumer_event_handler;
    if (epoll_add(&ctx->el, &io->et) < 0) {
	producer_destroy(sockfd);
	return NULL;
    }
    list_add(&io->io_link, &ctx->io_head);
    return io;
}

static proxyio_t *new_pingpong_comsumer(pingpong_ctx_t *ctx) {
    int *sockfd;
    proxyio_t *io;
    struct bc_opt *cf = ctx->cf;
    
    if (!(sockfd = comsumer_new(cf->host, cf->proxyname)))
	return NULL;
    io = container_of(sockfd, proxyio_t, sockfd);
    io->et.fd = *sockfd;
    io->et.events = EPOLLIN;
    io->et.f = comsumer_event_handler;
    if (epoll_add(&ctx->el, &io->et) < 0) {
	comsumer_destroy(sockfd);
	return NULL;
    }
    list_add(&io->io_link, &ctx->io_head);
    return io;
}


static inline int
producer_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    proxyio_t *io = container_of(et, proxyio_t, et);
    pingpong_ctx_t *ctx = container_of(el, pingpong_ctx_t, el);
    char *data;
    uint32_t sz;
    if (happened & EPOLLERR) {
	epoll_del(el, et);
	proxyio_destroy(io);
	new_pingpong_producer(ctx);
	return 0;
    }
    if (rand() % 234 == 0) {
	switch (rand() % 4) {
	case 0:
	    sk_write(io->sockfd, page, rand() % 100);
	    return -1;
	case 1:
	    epoll_del(el, et);
	    proxyio_destroy(io);
	    new_pingpong_producer(ctx);
	    return -1;
	}
    }
    if ((happened & EPOLLIN)
	&& producer_recv_response(&io->sockfd, &data, &sz) == 0) {
	mem_free(data, sz);
    }
    return 0;
}

static inline int
comsumer_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    proxyio_t *io = container_of(et, proxyio_t, et);
    pingpong_ctx_t *ctx = container_of(el, pingpong_ctx_t, el);
    char *data, *rt;
    uint32_t sz, rt_sz;
    if (happened & EPOLLERR) {
	epoll_del(el, et);
	proxyio_destroy(io);
	new_pingpong_comsumer(ctx);
	return 0;
    }
    if (rand() % 234 == 0) {
	switch (rand() % 4) {
	case 0:
	    sk_write(io->sockfd, page, rand() % 100);
	    return -1;
	case 1:
	    epoll_del(el, et);
	    proxyio_destroy(io);
	    new_pingpong_producer(ctx);
	    return -1;
	}
    }
    if (comsumer_recv_request(&io->sockfd, &data, &sz, &rt, &rt_sz) == 0) {
	comsumer_psend_response(&io->sockfd, data, sz, rt, rt_sz);
	mem_free(data, sz);
	mem_free(rt, rt_sz);
    }
    return 0;
}


int exception_start(struct bc_opt *cf) {
    int i;
    proxyio_t *io, *tmp;
    pingpong_ctx_t ctx = {};

    printf("exception benchmark start...\n");
    INIT_LIST_HEAD(&ctx.io_head);
    ctx.cf = cf;
    randstr(page, REQLEN);
    epoll_init(&ctx.el, 10240, 1024, 1);
    for (i = 0; i < cf->comsumer_num; i++)
	new_pingpong_comsumer(&ctx);
    for (i = 0; i < cf->producer_num; i++) {
	io = new_pingpong_producer(&ctx);
    }
    while (rt_mstime() < cf->deadline)
	epoll_oneloop(&ctx.el);
    list_for_each_pio_safe(io, tmp, &ctx.io_head) {
	epoll_del(&ctx.el, &io->et);
	list_del(&io->io_link);
	proxyio_destroy(io);
    }
    epoll_destroy(&ctx.el);
    return 0;
}
