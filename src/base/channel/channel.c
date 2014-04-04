#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "channel_base.h"

/* Backend poller wait kernel timeout msec */
#define PIO_POLLER_TIMEOUT 1

/* Default input/output buffer size */
static int PIO_SNDBUFSZ = 10485760;
static int PIO_RCVBUFSZ = 10485760;


struct channel_global cn_global = {};


extern int has_closed_channel(struct channel_poll *po);
extern void push_closed_channel(struct channel *cn);
extern struct channel *pop_closed_channel(struct channel_poll *po);


uint32_t msg_iovlen(char *payload) {
    struct channel_msg *msg = cont_of(payload, struct channel_msg, hdr.payload);
    return sizeof(msg->hdr) + msg->hdr.size;
}

char *msg_iovbase(char *payload) {
    struct channel_msg *msg = cont_of(payload, struct channel_msg, hdr.payload);
    return (char *)&msg->hdr;
}

char *channel_allocmsg(uint32_t size) {
    struct channel_msg *msg;
    char *chunk = (char *)mem_zalloc(sizeof(*msg) + size);
    if (!chunk)
	return NULL;
    msg = (struct channel_msg *)chunk;
    msg->hdr.size = size;
    msg->hdr.checksum = crc16((char *)&msg->hdr.size, 4);
    return msg->hdr.payload;
}

void channel_freemsg(char *payload) {
    struct channel_msg *msg = cont_of(payload, struct channel_msg, hdr.payload);
    mem_free(msg, sizeof(*msg) + msg->hdr.size);
}

static int choose_backend_poll(int cd) {
    return cd % cn_global.npolls;
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

    mutex_init(&cn->lock);
    condition_init(&cn->cond);
    cn->fasync = false;
    cn->fok = true;
    cn->parent = -1;
    cn->cd = cd;
    cn->pollid = choose_backend_poll(cd);
    cn->rg.events = 0;
    cn->rg.f = 0;
    cn->rg.self = 0;
    cn->rcv_waiters = 0;
    cn->snd_waiters = 0;
    cn->rcv = 0;
    cn->snd = 0;
    cn->rcv_wnd = PIO_RCVBUFSZ;
    cn->snd_wnd = PIO_SNDBUFSZ;
    INIT_LIST_HEAD(&cn->rcv_head);
    INIT_LIST_HEAD(&cn->snd_head);

    INIT_LIST_HEAD(&cn->closing_link);
    INIT_LIST_HEAD(&cn->err_link);
    INIT_LIST_HEAD(&cn->in_link);
    INIT_LIST_HEAD(&cn->out_link);
}

static void channel_base_exit(int cd) {
    struct channel *cn = cid_to_channel(cd);
    struct list_head head = {};
    struct channel_msg *pos, *nx;
    
    mutex_destroy(&cn->lock);
    condition_destroy(&cn->cond);
    cn->ty = -1;
    cn->pf = -1;
    cn->fasync = -1;
    cn->fok = -1;
    cn->cd = -1;
    cn->pollid = -1;
    cn->rg.events = -1;
    cn->rg.f = 0;
    cn->rg.self = 0;
    cn->rcv_waiters = -1;
    cn->snd_waiters = -1;
    cn->rcv = -1;
    cn->snd = -1;
    cn->rcv_wnd = -1;
    cn->snd_wnd = -1;

    INIT_LIST_HEAD(&head);
    list_splice(&cn->rcv_head, &head);
    list_splice(&cn->snd_head, &head);
    list_for_each_channel_msg_safe(pos, nx, &head)
	channel_freemsg(pos->hdr.payload);

    assert(!attached(&cn->closing_link));

    /* Detach from all poll status head */
    if (attached(&cn->err_link))
	list_del_init(&cn->err_link);
    if (attached(&cn->in_link))
	list_del_init(&cn->in_link);
    if (attached(&cn->out_link))
	list_del_init(&cn->out_link);
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

struct channel_poll *pid_to_channel_poll(int pd) {
    return &cn_global.polls[pd];
}

static inline int event_runner(void *args) {
    int rc = 0;
    int pd = alloc_pid();
    struct channel *closing_cn;
    struct channel_poll *po = pid_to_channel_poll(pd);

    spin_init(&po->lock);
    INIT_LIST_HEAD(&po->closing_head);
    INIT_LIST_HEAD(&po->error_head);
    INIT_LIST_HEAD(&po->readyin_head);
    INIT_LIST_HEAD(&po->readyout_head);

    /* Init eventloop */
    rc = eloop_init(&po->el, 10240, 1024, PIO_POLLER_TIMEOUT);
    assert(rc == 0);

    while (!cn_global.exiting || has_closed_channel(po)) {
	eloop_once(&po->el);
	while ((closing_cn = pop_closed_channel(po)))
	    closing_cn->vf->destroy(closing_cn->cd);
    }

    /* Release the poll descriptor when runner exit. */
    free_pid(pd);
    eloop_destroy(&po->el);
    spin_destroy(&po->lock);
    return rc;
}


extern struct channel_vf *inproc_channel_vfptr;
extern struct channel_vf *ipc_channel_vfptr;
extern struct channel_vf *tcp_channel_vfptr;


void global_channel_init() {
    int cd; /* Channel id */
    int pd; /* Poller id */
    int i;

    cn_global.exiting = false;
    mutex_init(&cn_global.lock);

    for (cd = 0; cd < PIO_MAX_CHANNELS; cd++)
	cn_global.unused[cd] = cd;
    for (pd = 0; pd < PIO_MAX_CPUS; pd++)
	cn_global.poll_unused[pd] = pd;

    cn_global.cpu_cores = 2;
    taskpool_init(&cn_global.tpool, cn_global.cpu_cores);
    taskpool_start(&cn_global.tpool);
    for (i = 0; i < cn_global.cpu_cores; i++)
	taskpool_run(&cn_global.tpool, event_runner, NULL);

    /* The priority of channel_vf: inproc > ipc > tcp */
    INIT_LIST_HEAD(global_vf_head);
    list_add_tail(&inproc_channel_vfptr->vf_item, global_vf_head);
    list_add_tail(&ipc_channel_vfptr->vf_item, global_vf_head);
    list_add_tail(&tcp_channel_vfptr->vf_item, global_vf_head);
}

void global_channel_exit() {
    cn_global.exiting = true;
    taskpool_stop(&cn_global.tpool);
    taskpool_destroy(&cn_global.tpool);
    mutex_destroy(&cn_global.lock);
}

void channel_close(int cd) {
    struct channel *cn = cid_to_channel(cd);
    push_closed_channel(cn);
}

int channel_accept(int cd) {
    struct channel *cn = cid_to_channel(cd);
    struct channel *new = alloc_channel();
    struct channel_vf *vf, *nx;

    new->ty = CHANNEL_ACCEPTER;
    new->pf = cn->pf;
    new->parent = cd;
    list_for_each_channel_vf_safe(vf, nx, &cn_global.channel_vf_head) {
	if ((cn->pf & vf->pf) && vf->init(new->cd) == 0) {
	    new->vf = vf;
	    return new->cd;
	}
    }
    free_channel(new);
    return -1;
}

int channel_listen(int pf, const char *addr) {
    struct channel *new = alloc_channel();
    struct channel_vf *vf, *nx;

    new->ty = CHANNEL_LISTENER;
    new->pf = pf;
    ZERO(new->addr);
    strncpy(new->addr, addr, TP_SOCKADDRLEN);
    list_for_each_channel_vf_safe(vf, nx, &cn_global.channel_vf_head) {
	if ((pf & vf->pf) && vf->init(new->cd) == 0) {
	    new->vf = vf;
	    return new->cd;
	}
    }
    free_channel(new);
    return -1;
}

int channel_connect(int pf, const char *peer) {
    struct channel *new = alloc_channel();
    struct channel_vf *vf, *nx;

    new->ty = CHANNEL_CONNECTOR;
    new->pf = pf;

    ZERO(new->peer);
    strncpy(new->peer, peer, TP_SOCKADDRLEN);
    list_for_each_channel_vf_safe(vf, nx, &cn_global.channel_vf_head) {
	if ((pf & vf->pf) && vf->init(new->cd) == 0) {
	    new->vf = vf;
	    return new->cd;
	}
    }
    free_channel(new);
    return -1;
}

int channel_setopt(int cd, int opt, void *val, int valsz) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    if (!val || valsz <= 0) {
	errno = EINVAL;
	return -1;
    }
    rc = cn->vf->setopt(cd, opt, val, valsz);
    return rc;
}

int channel_getopt(int cd, int opt, void *val, int valsz) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    if (!val || valsz <= 0) {
	errno = EINVAL;
	return -1;
    }
    rc = cn->vf->getopt(cd, opt, val, valsz);
    return rc;
}

int channel_recv(int cd, char **payload) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    if (!payload) {
	errno = EINVAL;
	return -1;
    }
    rc = cn->vf->recv(cd, payload);
    return rc;
}

int channel_send(int cd, char *payload) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    if (!payload) {
	errno = EINVAL;
	return -1;
    }
    rc = cn->vf->send(cd, payload);
    return rc;
}


int channel_poll(int cd, struct channel_events *ev) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    if (!ev || !ev->f || !ev->events) {
	errno = EINVAL;
	return -1;
    }
    mutex_lock(&cn->lock);
    cn->rg.events = ev->events;
    cn->rg.f = ev->f;
    cn->rg.self = ev->self;
    mutex_unlock(&cn->lock);
    return rc;
}
