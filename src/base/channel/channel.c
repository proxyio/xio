#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "channel.h"
#include "channel_base.h"

struct channel_global cn_global = {};


uint32_t channel_msgiov_len(struct channel_msg *msg) {
    struct channel_msg_item *mi = cont_of(msg, struct channel_msg_item, msg);	
    return sizeof(mi->hdr) + mi->hdr.payload_sz + mi->hdr.control_sz;
}

char *channel_msgiov_base(struct channel_msg *msg) {
    struct channel_msg_item *mi = cont_of(msg, struct channel_msg_item, msg);
    return (char *)&mi->hdr;
}

struct channel_msg *channel_allocmsg(uint32_t payload_sz, uint32_t control_sz) {
    struct channel_msg_item *mi;
    char *chunk = (char *)mem_zalloc(sizeof(*mi) + payload_sz + control_sz);
    if (!chunk)
	return NULL;
    mi = (struct channel_msg_item *)chunk;
    mi->hdr.payload_sz = payload_sz;
    mi->hdr.control_sz = control_sz;
    mi->hdr.checksum = crc16((char *)&mi->hdr.payload_sz, 16);
    mi->msg.payload = chunk + sizeof(*mi);
    mi->msg.control = mi->msg.payload + payload_sz;
    return &mi->msg;
}

void channel_freemsg(struct channel_msg *msg) {
    struct channel_msg_item *mi = cont_of(msg, struct channel_msg_item, msg);
    mem_free(mi, sizeof(*mi) + mi->hdr.payload_sz + mi->hdr.control_sz);
}

int alloc_cid() {
    int cd;
    mutex_lock(&cn_global.lock);
    assert(cn_global.nchannels < PIO_MAX_CHANNELS);
    cd = cn_global.unused[cn_global.nchannels++];
    mutex_unlock(&cn_global.lock);
    return cd;
}



void free_cid(int cd) {
    mutex_lock(&cn_global.lock);
    cn_global.unused[--cn_global.nchannels] = cd;
    mutex_unlock(&cn_global.lock);
}

struct channel *cid_to_channel(int cd) {
    return &cn_global.channels[cd];
}

static void channel_base_init(int cd) {
    struct channel *cn = cid_to_channel(cd);
    cn->fasync = false;
    cn->fok = true;
    cn->parent = -1;
    cn->cd = cd;
    cn->waiters = 0;
    mutex_init(&cn->lock);
    condition_init(&cn->cond);
    cn->rcv = 0;
    cn->snd = 0;
    cn->rcv_wnd = PIO_RCVBUFSZ;
    cn->snd_wnd = PIO_SNDBUFSZ;
    INIT_LIST_HEAD(&cn->rcv_head);
    INIT_LIST_HEAD(&cn->snd_head);
}

static void channel_base_exit(int cd) {
    struct channel *cn = cid_to_channel(cd);
    struct list_head head = {};
    struct channel_msg_item *pos, *nx;
    
    cn->ty = -1;
    cn->pf = -1;
    cn->fasync = -1;
    cn->fok = -1;
    cn->cd = -1;
    mutex_destroy(&cn->lock);
    condition_destroy(&cn->cond);
    cn->rcv = -1;
    cn->snd = -1;
    cn->rcv_wnd = -1;
    cn->snd_wnd = -1;

    INIT_LIST_HEAD(&head);
    list_splice(&cn->rcv_head, &head);
    list_splice(&cn->snd_head, &head);
    list_for_each_channel_msg_safe(pos, nx, &head)
	channel_freemsg(&pos->msg);
}


struct channel *alloc_channel() {
    int cd = alloc_cid();
    struct channel *cn = cid_to_channel(cd);
    channel_base_init(cd);
    return cn;
}

void free_channel(struct channel *cn) {
    int cd = cn->cd;
    channel_base_exit(cd);
    free_cid(cd);
}

int alloc_pid() {
    int pd;
    mutex_lock(&cn_global.lock);
    assert(cn_global.npolls < PIO_MAX_CPUS);
    pd = cn_global.poll_unused[cn_global.npolls++];
    mutex_unlock(&cn_global.lock);
    return pd;
}

void free_pid(int pd) {
    mutex_lock(&cn_global.lock);
    cn_global.poll_unused[--cn_global.npolls] = pd;
    mutex_unlock(&cn_global.lock);
}

epoll_t *pid_to_poller(int pd) {
    return &cn_global.polls[pd];
}

void global_put_closing_channel(struct channel *cn) {
    struct list_head *head = &cn_global.closing_head[cn->pd];

    mutex_lock(&cn_global.lock);
    list_add_tail(&cn->closing_link, head);
    mutex_unlock(&cn_global.lock);
}

struct channel *global_get_closing_channel(int pd) {
    struct channel *cn = NULL;
    struct list_head *head = &cn_global.closing_head[pd];

    mutex_lock(&cn_global.lock);
    if (!list_empty(head))
	cn = list_first(head, struct channel, closing_link);
    mutex_unlock(&cn_global.lock);
    return cn;
}

static inline int event_runner(void *args) {
    int rc = 0;
    int pd = alloc_pid();
    epoll_t *el = pid_to_poller(pd);
    struct channel *closing_cn;

    assert(epoll_init(el, 10240, 1024, PIO_POLLER_TIMEOUT) == 0);
    while (!cn_global.exiting) {
	epoll_oneloop(el);
	while ((closing_cn = global_get_closing_channel(pd)) != NULL) {
	    closing_cn->vf->destroy(closing_cn->cd);
	    free_channel(closing_cn);
	}
    }
    while ((closing_cn = global_get_closing_channel(pd)) != NULL) {
	closing_cn->vf->destroy(closing_cn->cd);
	free_channel(closing_cn);
    }

    /*  Release the poller descriptor to global table when runner exit.  */
    free_pid(pd);
    epoll_destroy(el);
    return rc;
}

int select_a_poller(int cd) {
    return cd % cn_global.npolls;
}

void global_channel_init() {
    int cd;
    int pd;
    int i;

    cn_global.exiting = false;

    mutex_init(&cn_global.lock);

    for (cd = 0; cd < PIO_MAX_CHANNELS; cd++)
	cn_global.unused[cd] = cd;
    for (pd = 0; pd < PIO_MAX_CPUS; pd++) {
	cn_global.poll_unused[pd] = pd;
	INIT_LIST_HEAD(&cn_global.closing_head[pd]);
	INIT_LIST_HEAD(&cn_global.error_head[pd]);
	INIT_LIST_HEAD(&cn_global.readyin_head[pd]);
	INIT_LIST_HEAD(&cn_global.readyout_head[pd]);
    }
    cn_global.cpu_cores = 2;
    taskpool_init(&cn_global.tpool, cn_global.cpu_cores);
    taskpool_start(&cn_global.tpool);
    for (i = 0; i < cn_global.cpu_cores; i++)
	taskpool_run(&cn_global.tpool, event_runner, NULL);
}

void global_channel_exit() {
    cn_global.exiting = true;
    taskpool_stop(&cn_global.tpool);
    taskpool_destroy(&cn_global.tpool);
    mutex_destroy(&cn_global.lock);
}

void channel_close(int cd) {
    struct channel *cn = cid_to_channel(cd);
    cn->vf->close(cd);
}

int channel_accept(int cd) {
    struct channel *cn = cid_to_channel(cd);
    struct channel_vf *vf = cn->vf;
    struct channel *new = alloc_channel();

    new->ty = CHANNEL_ACCEPTER;
    new->pf = cn->pf;
    new->vf = vf;
    new->parent = cd;
    vf->init(new->cd);
    return new->cd;
}

int channel_listen(int pf, const char *sock) {
    struct channel *new = alloc_channel();
    struct channel_vf *vf = (pf == PF_INPROC) ? inproc_channel_vfptr :
	io_channel_vfptr;

    new->ty = CHANNEL_LISTENER;
    new->pf = pf;
    new->vf = vf;
    strncpy(new->sock, sock, TP_SOCKADDRLEN);
    vf->init(new->cd);
    return new->cd;
}

int channel_connect(int pf, const char *peer) {
    struct channel *new = alloc_channel();
    struct channel_vf *vf = (pf == PF_INPROC) ? inproc_channel_vfptr :
	io_channel_vfptr;

    new->ty = CHANNEL_CONNECTOR;
    new->pf = pf;
    new->vf = vf;
    strncpy(new->peer, peer, TP_SOCKADDRLEN);
    vf->init(new->cd);
    return new->cd;
}

int channel_setopt(int cd, int opt, void *val, int valsz) {
    struct channel *cn = cid_to_channel(cd);
    return cn->vf->setopt(cd, opt, val, valsz);
}

int channel_getopt(int cd, int opt, void *val, int valsz) {
    struct channel *cn = cid_to_channel(cd);
    return cn->vf->getopt(cd, opt, val, valsz);
}

int channel_recv(int cd, struct channel_msg **msg) {
    struct channel *cn = cid_to_channel(cd);
    return cn->vf->recv(cd, msg);
}

int channel_send(int cd, struct channel_msg *msg) {
    struct channel *cn = cid_to_channel(cd);
    return cn->vf->send(cd, msg);
}
