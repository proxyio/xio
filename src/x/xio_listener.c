#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "xbase.h"

extern struct io default_xops;

/***************************************************************************
 *  request_socks events trigger.
 ***************************************************************************/

static void request_socks_full(int xd) {
    struct xsock *sx = xget(xd);    
    struct xcpu *cpu = xcpuget(sx->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if ((sx->io.et.events & EPOLLIN)) {
	sx->io.et.events &= ~EPOLLIN;
	BUG_ON(eloop_mod(&cpu->el, &sx->io.et) != 0);
    }
}

static void request_socks_nonfull(int xd) {
    struct xsock *sx = xget(xd);    
    struct xcpu *cpu = xcpuget(sx->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if (!(sx->io.et.events & EPOLLIN)) {
	sx->io.et.events |= EPOLLIN;
	BUG_ON(eloop_mod(&cpu->el, &sx->io.et) != 0);
    }
}

static int xio_listener_handler(eloop_t *el, ev_t *et);

static int xio_listener_bind(int pf, const char *sock) {
    int s, on = 1;
    struct xsock *sx = 0;
    struct xcpu *cpu = 0;
    struct transport *tp = transport_lookup(pf);

    BUG_ON(!tp);
    if ((s = tp->bind(sock)) < 0 || !(sx = xsock_alloc()))
	return -1;

    ZERO(sx->io);
    sx->pf = pf;
    sx->l4proto = l4proto_lookup(pf, XLISTENER);
    strncpy(sx->addr, sock, TP_SOCKADDRLEN);

    tp->setopt(s, TP_NOBLOCK, &on, sizeof(on));
    sx->io.et.events = EPOLLIN|EPOLLERR;
    sx->io.et.fd = s;
    sx->io.et.f = xio_listener_handler;
    sx->io.et.data = sx;
    sx->io.fd = s;
    sx->io.tp = tp;

    cpu = xcpuget(sx->cpu_no);
    BUG_ON(eloop_add(&cpu->el, &sx->io.et) != 0);
    return sx->xd;
}

static void xio_listener_close(int xd) {
    struct xsock *sx = xget(xd);
    struct xcpu *cpu = xcpuget(sx->cpu_no);
    struct transport *tp = sx->io.tp;

    BUG_ON(!tp);

    /* Detach xsock low-level file descriptor from poller */
    BUG_ON(eloop_del(&cpu->el, &sx->io.et) != 0);
    tp->close(sx->io.fd);

    sx->io.fd = -1;
    sx->io.et.events = -1;
    sx->io.et.fd = -1;
    sx->io.et.f = 0;
    sx->io.et.data = 0;
    sx->io.tp = 0;

    /* Destroy the xsock base and free xsockid. */
    xsock_free(sx);
}


static void request_socks_notify(int xd, uint32_t events) {
    if (events & XMQ_FULL)
	request_socks_full(xd);
    else if (events & XMQ_NONFULL)
	request_socks_nonfull(xd);
}

extern int xio_connector_handler(eloop_t *el, ev_t *et);

static int xio_listener_handler(eloop_t *el, ev_t *et) {
    int rc = 0;
    int on = 1;
    struct xsock *sx = cont_of(et, struct xsock, io.et);
    struct transport *tp = sx->io.tp;
    struct xsock *req_sx = xsock_alloc();
    struct xcpu *cpu = xcpuget(req_sx->cpu_no);
    
    if (!req_sx) {
	errno = EAGAIN;
	return -1;
    }
    ZERO(req_sx->io);
    bio_init(&req_sx->io.in);
    bio_init(&req_sx->io.out);
    if ((et->happened & EPOLLERR) || !(et->happened & EPOLLIN)) {
	sx->fok = false;
	errno = EPIPE;
	return -1;
    } 

    /* Accept new connection */
    if ((req_sx->io.fd = tp->accept(sx->io.fd)) < 0)
	return -1;
    DEBUG_OFF("xsock accept new connection %d", req_sx->io.fd);
    tp->setopt(req_sx->io.fd, TP_NOBLOCK, &on, sizeof(on));

    req_sx->pf = sx->pf;
    req_sx->l4proto = l4proto_lookup(sx->pf, XCONNECTOR);
    req_sx->io.et.events = EPOLLIN|EPOLLRDHUP|EPOLLERR;
    req_sx->io.et.fd = req_sx->io.fd;
    req_sx->io.et.f = xio_connector_handler;
    req_sx->io.et.data = req_sx;
    req_sx->io.tp = tp;
    req_sx->io.ops = default_xops;
    BUG_ON(eloop_add(&cpu->el, &req_sx->io.et) != 0);
    push_request_sock(sx, req_sx);
    return rc;
}

struct xsock_protocol xtcp_listener_protocol = {
    .type = XLISTENER,
    .pf = PF_NET,
    .bind = xio_listener_bind,
    .close = xio_listener_close,
    .rcv_notify = request_socks_notify,
    .snd_notify = null,
};

struct xsock_protocol xipc_listener_protocol = {
    .type = XLISTENER,
    .pf = PF_IPC,
    .bind = xio_listener_bind,
    .close = xio_listener_close,
    .rcv_notify = request_socks_notify,
    .snd_notify = null,
};
