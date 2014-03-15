#include <stdio.h>
#include "opt.h"
#include "os/epoll.h"
#include "os/timestamp.h"
#include "sdk/c/proxyio.h"

#define REQLEN (PAGE_SIZE * 8)
extern int randstr(char *buff, int sz);

static inline int
producer_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    proxyio_t *io = container_of(et, proxyio_t, et);
    char *data;
    uint32_t sz;
    BUG_ON(happened != EPOLLIN);

    if (producer_recv_response(&io->sockfd, &data, &sz) == 0) {
	producer_psend_request(&io->sockfd, data, sz);
	mem_free(data, sz);
    }
    return 0;
}

static inline int
comsumer_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    proxyio_t *io = container_of(et, proxyio_t, et);
    char *data, *rt;
    uint32_t sz, rt_sz;
    BUG_ON(happened != EPOLLIN);

    if (comsumer_recv_request(&io->sockfd, &data, &sz, &rt, &rt_sz) == 0) {
	comsumer_psend_response(&io->sockfd, data, sz, rt, rt_sz);
	mem_free(data, sz);
	mem_free(rt, rt_sz);
    }
    return 0;
}


int pingpong_start(struct bc_opt *cf) {
    int i;
    struct list_head io_head = {};
    uint32_t sz;
    char page[REQLEN];
    proxyio_t *io, *tmp;
    int *sockfd;
    epoll_t el = {};

    INIT_LIST_HEAD(&io_head);
    randstr(page, REQLEN);
    epoll_init(&el, 10240, 1024, 1);
    for (i = 0; i < cf->comsumer_num; i++) {
	if (!(sockfd = comsumer_new(cf->host, cf->proxyname)))
	    continue;
	io = container_of(io, proxyio_t, sockfd);
	io->et.fd = *sockfd;
	io->et.events = EPOLLIN;
	io->et.f = comsumer_event_handler;
	if (epoll_add(&el, &io->et) < 0) {
	    producer_destroy(sockfd);
	    continue;
	}
	list_add(&io->io_link, &io_head);
    }
    for (i = 0; i < cf->producer_num; i++) {
	if (!(sockfd = producer_new(cf->host, cf->proxyname)))
	    continue;
	io = container_of(io, proxyio_t, sockfd);
	io->et.fd = *sockfd;
	io->et.events = EPOLLIN;
	io->et.f = producer_event_handler;
	if (epoll_add(&el, &io->et) < 0) {
	    producer_destroy(sockfd);
	    continue;
	}
	list_add(&io->io_link, &io_head);
	sz = rand() % REQLEN;
	BUG_ON(producer_psend_request(sockfd, page, sz) != 0);
    }
    while (rt_mstime() < cf->deadline)
	epoll_oneloop(&el);
    list_for_each_pio_safe(io, tmp, &io_head) {
	epoll_del(&el, &io->et);
	list_del(&io->io_link);
	proxyio_destroy(io);
    }
    epoll_destroy(&el);
    return 0;
}
