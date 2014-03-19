#include <stdio.h>
#include "transport/tcp/tcp.h"
#include "role.h"
#include "proxy.h"
#include "runner/taskpool.h"

static inline epoll_t *acp_get_subel_rrbin(acp_t *acp) {
    epoll_t *sub_el;
    lock(acp);
    if (!list_empty(&acp->sub_el_head)) {
	sub_el = list_first(&acp->sub_el_head, epoll_t, link);
	list_del(&sub_el->link);
	list_add_tail(&sub_el->link, &acp->sub_el_head);
    } else
	sub_el = &acp->main_el;
    unlock(acp);
    return sub_el;
}

static inline int acp_event_handler(epoll_t *el, epollevent_t *et) {
    acp_t *acp = container_of(el, struct accepter, main_el);
    int nfd;
    struct role *r;

    if (!(et->happened & EPOLLIN) || (nfd = act_accept(et->fd)) < 0)
	return -1;
    sk_setopt(nfd, SK_NONBLOCK, true);
    if (!(r = r_new_inited())) {
	close(nfd);
	return -1;
    }
    r->proxyto = false;
    r->el = acp_get_subel_rrbin(acp);
    r->et.fd = r->pp.sockfd = nfd;
    r->et.events = EPOLLIN|EPOLLOUT;
    r->acp = acp;
    if (epoll_add(r->el, &r->et) < 0) {
	close(nfd);
	r_destroy(r);
	mem_free(r, sizeof(*r));
	return -1;
    }
    return 0;
}


void acp_init(acp_t *acp, const struct cf *cf) {
    epoll_t *sub_el;
    int cpus = cf->max_cpus;

    INIT_LIST_HEAD(&acp->sub_el_head);
    INIT_LIST_HEAD(&acp->et_head);
    INIT_LIST_HEAD(&acp->py_head);
    acp->cf = *cf;
    spin_init(&acp->lock);
    epoll_init(&acp->main_el, 10240, cf->el_io_size, cf->el_wait_timeout);
    list_add(&acp->main_el.link, &acp->sub_el_head);
    while (--cpus > 0) {
	if (!(sub_el = epoll_new()))
	    continue;
	epoll_init(sub_el, 10240, cf->el_io_size, cf->el_wait_timeout);
	list_add(&sub_el->link, &acp->sub_el_head);
    }
    taskpool_init(&acp->tp, cf->max_cpus);
}

void acp_destroy(acp_t *acp) {
    epoll_t *el, *eltmp;
    epollevent_t *et, *ettmp;
    proxy_t *py, *pytmp;
    
    list_for_each_el_safe(el, eltmp, &acp->sub_el_head) {
	list_del(&el->link);
	epoll_destroy(el);
	if (el == &acp->main_el)
	    continue;
	mem_free(el, sizeof(*el));
    }
    list_for_each_et_safe(et, ettmp, &acp->et_head) {
	list_del(&et->el_link);
	epoll_del(&acp->main_el, et);
	close(et->fd);
	mem_free(et, sizeof(*et));
    }
    list_for_each_py_safe(py, pytmp, &acp->py_head) {
	list_del(&py->acp_link);
	proxy_destroy(py);
	mem_free(py, sizeof(*py));
    }
    spin_destroy(&acp->lock);	
    taskpool_destroy(&acp->tp);	
}


static inline int acp_reactor(void *args) {
    epoll_t *el = (epoll_t *)args;
    return epoll_startloop(el);
}

void acp_start(acp_t *acp) {
    epoll_t *sub_el, *tmp;

    lock(acp);
    taskpool_start(&acp->tp);
    list_for_each_el_safe(sub_el, tmp, &acp->sub_el_head)
	taskpool_run(&acp->tp, acp_reactor, sub_el);
    unlock(acp);
}

void acp_stop(acp_t *acp) {
    epoll_t *sub_el, *tmp;

    lock(acp);
    epoll_stoploop(&acp->main_el);
    list_for_each_el_safe(sub_el, tmp, &acp->sub_el_head)
	epoll_stoploop(sub_el);
    taskpool_stop(&acp->tp);
    unlock(acp);
}

int acp_listen(acp_t *acp, const char *addr) {
    epollevent_t *et = epollevent_new();

    if (!et)
	return -1;
    if ((et->fd = act_listen("tcp", addr, 1024)) < 0) {
	mem_free(et, sizeof(*et));
	return -1;
    }
    lock(acp);
    et->f = acp_event_handler;
    et->events = EPOLLIN;
    list_add(&et->el_link, &acp->et_head);
    if (epoll_add(&acp->main_el, et) < 0) {
	list_del(&et->el_link);
	close(et->fd);
	mem_free(et, sizeof(*et));
	unlock(acp);
	return -1;
    }
    unlock(acp);
    return 0;
}

static inline proxy_t *__acp_find(acp_t *acp, const char *pyn) {
    proxy_t *py;

    list_for_each_entry(py, &acp->py_head, proxy_t, acp_link)
	if (strncmp(py->name, pyn, PROXYNAME_MAX) == 0)
	    return py;
    return NULL;
}

proxy_t *acp_getpy(acp_t *acp, const char *pyn) {
    proxy_t *py;

    lock(acp);
    if ((py = __acp_find(acp, pyn)) || !(py = proxy_new()))
	goto EXIT;
    proxy_init(py);
    strncpy(py->name, pyn, PROXYNAME_MAX);
    list_add(&py->acp_link, &acp->py_head);
 EXIT:
    unlock(acp);
    return py;
}

int acp_proxyto(acp_t *acp, const char *pyn, const char *addr) {
    int nfd;
    pio_rgh_t *h;
    struct role *r;
    
    if ((nfd = sk_connect("tcp", "", addr)) < 0)
	return -1;
    if (!(r = r_new_inited())) {
	close(nfd);
	return -1;
    }
    lock(acp);
    sk_setopt(nfd, SK_NONBLOCK, true);
    h = &r->pp.rgh;
    h->type = PIO_RCVER;
    uuid_generate(h->id);
    strcpy(h->proxyname, pyn);
    r->proxyto = true;
    r->el = acp_get_subel_rrbin(acp);
    r->et.fd = r->pp.sockfd = nfd;
    r->et.events = EPOLLOUT;
    r->acp = acp;
    bio_write(&r->pp.out, (char *)h, sizeof(*h));
    h->type = PIO_SNDER;
    if (epoll_add(r->el, &r->et) < 0) {
	close(nfd);
	r_destroy(r);
	mem_free(r, sizeof(*r));
	unlock(acp);
	return -1;
    }
    unlock(acp);
    return 0;
}
