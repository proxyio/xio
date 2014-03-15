#include <stdio.h>
#include "net/accepter.h"
#include "role.h"
#include "proxy.h"
#include "runner/taskpool.h"

static inline int acp_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    int nfd;
    struct role *r;
    if (!(happened & EPOLLIN) || (nfd = act_accept(et->fd)) < 0)
	return -1;
    sk_setopt(nfd, SK_NONBLOCK, true);
    if (!(r = r_new_inited())) {
	close(nfd);
	return -1;
    }
    r->is_register = true;
    r->el = el;
    r->et.fd = r->io.sockfd = nfd;
    r->et.events = EPOLLIN;
    if (epoll_add(el, &r->et) < 0) {
	close(nfd);
	r_destroy(r);
	mem_free(r, sizeof(*r));
	return -1;
    }
    return 0;
}


void acp_init(acp_t *acp, const struct cf *cf) {
    acp->cf = *cf;
    spin_init(&(acp)->lock);
    epoll_init(&(acp)->el, 10240, cf->el_io_size, cf->el_wait_timeout);
    taskpool_init(&(acp)->tp, cf->tp_workers);
    INIT_LIST_HEAD(&(acp)->et_head);
    INIT_LIST_HEAD(&(acp)->py_head);
}

void acp_destroy(acp_t *acp) {
    epollevent_t *et, *tmp;
    list_for_each_et_safe(et, tmp, &acp->et_head) {
	epoll_del(&acp->el, et);
	list_del(&et->el_link);
	mem_free(et, sizeof(*et));
    }
    epoll_destroy(&(acp)->el);	
    spin_destroy(&(acp)->lock);	
    taskpool_destroy(&(acp)->tp);	
}

proxy_t *acp_find(acp_t *acp, char proxyname[PROXYNAME_MAX]) {
    proxy_t *cur, *match = NULL;

    list_for_each_entry(cur, &acp->py_head, proxy_t, acp_link) {
	if (memcmp(proxyname, cur->proxyname, PROXYNAME_MAX) != 0)
	    continue;
	match = cur;
	break;
    }
    return match;
}

static inline int acp_worker(void *args) {
    acp_t *acp = (acp_t *)args;
    return epoll_startloop(&acp->el);
}

void acp_start(acp_t *acp) {
    taskpool_start(&acp->tp);
    taskpool_run(&acp->tp, acp_worker, acp);
}

void acp_stop(acp_t *acp) {
    epoll_stoploop(&acp->el);
    taskpool_stop(&acp->tp);
}

int acp_listen(acp_t *acp, const char *addr) {
    epollevent_t *et = epollevent_new();

    if (!et)
	return -1;
    if ((et->fd = act_listen("tcp", addr, 1024)) < 0) {
	mem_free(et, sizeof(*et));
	return -1;
    }
    et->f = acp_event_handler;
    et->data = acp;
    et->events = EPOLLIN;
    list_add(&et->el_link, &acp->et_head);
    return epoll_add(&acp->el, et);
}

int acp_proxy(acp_t *acp, char *proxyname) {
    proxy_t *py = proxy_new();

    if (!py)
	return -1;
    proxy_init(py);
    strncpy(py->proxyname, proxyname, PROXYNAME_MAX);
    list_add(&py->acp_link, &acp->py_head);
    return 0;
}

int acp_proxyto(acp_t *acp, char *proxyname, const char *addr) {
    int nfd;
    pio_rgh_t *h;
    struct role *r;
    
    if ((nfd = sk_connect("tcp", "", addr)) < 0)
	return -1;
    if (!(r = r_new_inited())) {
	close(nfd);
	return -1;
    }
    sk_setopt(nfd, SK_NONBLOCK, true);
    h = &r->io.rgh;
    h->type = PIO_RCVER;
    uuid_generate(h->id);
    strcpy(h->proxyname, proxyname);
    r->is_register = false;
    r->el = &acp->el;
    r->et.fd = r->io.sockfd = nfd;
    r->et.events = EPOLLOUT;
    bio_write(&r->io.out, (char *)h, sizeof(*h));
    h->type = PIO_SNDER;
    if (epoll_add(&acp->el, &r->et) < 0) {
	close(nfd);
	r_destroy(r);
	mem_free(r, sizeof(*r));
	return -1;
    }
    return 0;
}
