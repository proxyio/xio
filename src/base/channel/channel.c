#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "channel.h"
#include "channel_base.h"

struct channel_global cn_global = {};


uint32_t channel_msgiov_len(struct channel_msg *msg) {
    struct channel_msg_item *msgi = pio_cont(msg, struct channel_msg_item, msg);	
    return sizeof(msgi->hdr) + msgi->hdr.payload_sz + msgi->hdr.control_sz;		
}

char *channel_msgiov_base(struct channel_msg *msg) {
    struct channel_msg_item *msgi = pio_cont(msg, struct channel_msg_item, msg);
    return (char *)&msgi->hdr;				
}

static inline int global_get_channel_id() {
    int cd;
    mutex_lock(&cn_global.lock);
    assert(cn_global.nchannels < PIO_MAX_CHANNELS);
    cd = cn_global.unused[cn_global.nchannels++];
    mutex_unlock(&cn_global.lock);
    return cd;
}

static inline void global_put_channel_id(int cd) {
    mutex_lock(&cn_global.lock);
    cn_global.unused[--cn_global.nchannels] = cd;
    mutex_unlock(&cn_global.lock);
}

static inline struct channel *global_channel(int cd) {
    return &cn_global.channels[cd];
}


static inline int global_get_poller_id() {
    int pd;
    mutex_lock(&cn_global.lock);
    assert(cn_global.npolls < PIO_MAX_CPUS);
    pd = cn_global.poll_unused[cn_global.npolls++];
    mutex_unlock(&cn_global.lock);
    return pd;
}

static inline void global_put_poller_id(int pd) {
    mutex_lock(&cn_global.lock);
    cn_global.poll_unused[--cn_global.npolls] = pd;
    mutex_unlock(&cn_global.lock);
}

static inline epoll_t *global_poller(int pd) {
    return &cn_global.polls[pd];
}

static inline void global_put_closing_channel(struct channel *cn) {
    struct list_head *head = &cn_global.closing_head[cn->pd];

    mutex_lock(&cn_global.lock);
    list_add_tail(&cn->closing_link, head);
    mutex_unlock(&cn_global.lock);
}

static inline struct channel *global_get_closing_channel(int pd) {
    struct channel *cn = NULL;
    struct list_head *head = &cn_global.closing_head[pd];

    mutex_lock(&cn_global.lock);
    if (!list_empty(head))
	cn = list_first(head, struct channel, closing_link);
    mutex_unlock(&cn_global.lock);
    return cn;
}

static inline void global_channel_poll_state(struct channel *cn) {
    struct list_head *err_head = &cn_global.error_head[cn->pd];
    struct list_head *in_head = &cn_global.readyin_head[cn->pd];
    struct list_head *out_head = &cn_global.readyout_head[cn->pd];
    
    mutex_lock(&cn_global.lock);
    mutex_lock(&cn->lock);

    /* Check the error status */
    if (!cn->fok && !attached(&cn->err_link))
	list_add_tail(&cn->err_link, err_head);

    /* Check the readyin status */
    if (attached(&cn->in_link) && list_empty(&cn->rcv_head))
	list_del_init(&cn->in_link);
    else if (!attached(&cn->in_link) && !list_empty(&cn->rcv_head))
	list_add_tail(&cn->in_link, in_head);

    /* Check the readyout status */
    if (attached(&cn->out_link) && cn->out.bsize > cn->snd_bufsz)
	list_del_init(&cn->out_link);
    else if (!attached(&cn->out_link) && cn->out.bsize <= cn->snd_bufsz)
	list_add_tail(&cn->out_link, out_head);
    
    mutex_unlock(&cn->lock);
    mutex_unlock(&cn_global.lock);
}


static void channel_destroy(struct channel *cn);

static inline int poller_runner(void *args) {
    int rc = 0;
    int pd = global_get_poller_id();
    epoll_t *el = global_poller(pd);
    struct channel *closing_cn;

    assert(epoll_init(el, 10240, 1024, PIO_POLLER_TIMEOUT) == 0);
    while (!cn_global.exiting) {
	epoll_oneloop(el);
	while ((closing_cn = global_get_closing_channel(pd)) != NULL)
	    channel_destroy(closing_cn);
    }
    while ((closing_cn = global_get_closing_channel(pd)) != NULL)
	channel_destroy(closing_cn);

    /*  Release the poller descriptor to global table when runner exit.  */
    global_put_poller_id(pd);
    epoll_destroy(el);
    return rc;
}

static inline int select_a_poller(int cd) {
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
	taskpool_run(&cn_global.tpool, poller_runner, NULL);
}

void global_channel_exit() {
    cn_global.exiting = true;
    taskpool_stop(&cn_global.tpool);
    taskpool_destroy(&cn_global.tpool);
    mutex_destroy(&cn_global.lock);
}



static int64_t channel_io_read(struct io *io_ops, char *buff, int64_t sz) {
    int rc;
    struct channel *cn = pio_cont(io_ops, struct channel, sock_ops);
    rc = cn->tp->read(cn->fd, buff, sz);
    return rc;
}

static int64_t channel_io_write(struct io *io_ops, char *buff, int64_t sz) {
    int rc;
    struct channel *cn = pio_cont(io_ops, struct channel, sock_ops);
    rc = cn->tp->write(cn->fd, buff, sz);
    return rc;
}

static struct io default_channel_ops = {
    .read = channel_io_read,
    .write = channel_io_write,
};


static int io_event_handler(epoll_t *el, epollevent_t *et);
static int accepter_event_handler(epoll_t *el, epollevent_t *et);


struct channel_msg *channel_allocmsg(uint32_t payload_sz, uint32_t control_sz) {
    struct channel_msg_item *msgi;
    char *chunk = (char *)mem_zalloc(sizeof(*msgi) + payload_sz + control_sz);
    if (!chunk)
	return NULL;
    msgi = (struct channel_msg_item *)chunk;
    msgi->hdr.payload_sz = payload_sz;
    msgi->hdr.control_sz = control_sz;
    msgi->hdr.checksum = crc16((char *)&msgi->hdr.payload_sz, 16);
    msgi->msg.payload = chunk + sizeof(*msgi);
    msgi->msg.control = msgi->msg.payload + payload_sz;
    return &msgi->msg;
}

void channel_freemsg(struct channel_msg *msg) {
    struct channel_msg_item *msgi = container_of(msg, struct channel_msg_item, msg);
    mem_free(msgi, sizeof(*msgi) + msgi->hdr.payload_sz + msgi->hdr.control_sz);
}

static void channel_init(int cd, int isaccepter, int fd, struct transport *tp) {
    struct channel *cn = global_channel(cd);

    cn->fasync = false;
    cn->fok = true;
    cn->faccepter = !!isaccepter;
    
    /* Init channel id */
    cn->cd = cd;

    /* Init lock */
    mutex_init(&cn->lock);
    condition_init(&cn->cond);
    
    /* Init caching msg head */
    cn->rcv_bufsz = PIO_RCVBUFSZ;
    cn->snd_bufsz = PIO_SNDBUFSZ;
    INIT_LIST_HEAD(&cn->rcv_head);
    INIT_LIST_HEAD(&cn->snd_head);

    /* Init poll entry */
    cn->et.events = EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLERR;
    cn->et.fd = fd;
    cn->et.f = isaccepter ? io_event_handler : accepter_event_handler;
    cn->et.data = cn;
    cn->pd = select_a_poller(cd);
    
    /* Init input/output buffer */
    bio_init(&cn->in);
    bio_init(&cn->out);
    cn->sock_ops = default_channel_ops;

    /* Init low-level transport vfptr */
    cn->fd = fd;
    cn->tp = tp;
    
    /* Init backend poll link */
    INIT_LIST_HEAD(&cn->closing_link);
    INIT_LIST_HEAD(&cn->err_link);
    INIT_LIST_HEAD(&cn->in_link);
    INIT_LIST_HEAD(&cn->out_link);

    /* Add current channel into async io poller */
    assert(epoll_add(global_poller(cn->pd), &cn->et) == 0);
}

static void channel_destroy(struct channel *cn) {
    struct list_head head;
    struct channel_msg_item *item, *nx;

    /* Destroy lock */
    mutex_destroy(&cn->lock);
    condition_destroy(&cn->cond);
    
    /* Release all the caching msg */
    INIT_LIST_HEAD(&head);
    list_splice(&cn->rcv_head, &head);
    list_splice(&cn->snd_head, &head);
    list_for_each_channel_msg_safe(item, nx, &head)
	channel_freemsg(&item->msg);

    /* Detach channel low-level file descriptor from poller */
    assert(epoll_del(global_poller(cn->pd), &cn->et) == 0);
    cn->pd = -1;
    
    /*  Destroy buffer io  */
    bio_destroy(&cn->in);
    bio_destroy(&cn->out);

    /* close low-level transport file descriptor */
    cn->tp->close(cn->fd);
    cn->fd = -1;

    /* Detach from all poll status head */
    if (attached(&cn->closing_link))
	list_del_init(&cn->closing_link);
    if (attached(&cn->err_link))
	list_del_init(&cn->err_link);
    if (attached(&cn->in_link))
	list_del_init(&cn->in_link);
    if (attached(&cn->out_link))
	list_del_init(&cn->out_link);

    /* Remove this channel from the table, add it to unused channel
       table. */
    global_put_channel_id(cn->cd);
    cn->cd = -1;
}


void channel_close(int cd) {
    global_put_closing_channel(global_channel(cd));
}


int channel_listen(int pf, const char *addr) {
    int cd, s;
    struct transport *tp = transport_lookup(pf);

    if (!tp)
	return -EINVAL;
    if ((s = tp->bind(addr)) < 0)
	return s;

    // Find a unused channel id and slot
    cd = global_get_channel_id();

    // Init channel from raw-level sockid and transport vfptr
    channel_init(cd, false, s, tp);
    return cd;
}

int channel_connect(int pf, const char *peer) {
    int cd, s;
    int ff = 1;
    struct transport *tp = transport_lookup(pf);

    if (!tp)
	return -EINVAL;
    if ((s = tp->connect(peer)) < 0)
	return s;
    
    // Find a unused channel id and slot
    cd = global_get_channel_id();

    // Init channel from raw-level sockid and transport vfptr
    channel_init(cd, true, s, tp);
    tp->setopt(s, PIO_NONBLOCK, &ff, sizeof(ff));

    return cd;
}

int channel_accept(int cd) {
    int new_cd, s;
    int ff = 1;
    struct channel *cn = global_channel(cd);
    struct transport *tp = cn->tp;

    if ((s = tp->accept(cn->fd)) < 0)
	return s;

    // Find a unused channel id and slot.
    new_cd = global_get_channel_id();

    // Init channel from raw-level sockid and transport vfptr.
    channel_init(new_cd, true, s, tp);
    tp->setopt(s, PIO_NONBLOCK, &ff, sizeof(ff));

    return new_cd;
}

int channel_setopt(int cd, int opt, void *val, int valsz) {
    int rc = 0;
    struct channel *cn = global_channel(cd);
    struct transport *tp = cn->tp;

    mutex_lock(&cn->lock);

    if (opt == PIO_NONBLOCK) {
	if (!cn->fasync && (*(int *)val))
	    cn->fasync = true;
	else if (cn->fasync && (*(int *)val) == 0)
	    cn->fasync = false;
    } else
	rc = tp->setopt(cn->fd, opt, val, valsz);

    mutex_unlock(&cn->lock);
    return rc;
}

int channel_getopt(int cd, int opt, void *val, int valsz) {
    int rc;
    struct channel *cn = global_channel(cd);
    struct transport *tp = cn->tp;

    mutex_lock(&cn->lock);

    rc = tp->getopt(cn->fd, opt, val, valsz);

    mutex_unlock(&cn->lock);
    return rc;
}




static void channel_push_rcvmsg(struct channel *cn, struct channel_msg *msg) {
    struct channel_msg_item *msgi = pio_cont(msg, struct channel_msg_item, msg);

    mutex_lock(&cn->lock);
    list_add_tail(&msgi->item, &cn->rcv_head);

    /* Wakeup the blocking waiters. fix by needed here */
    condition_broadcast(&cn->cond);
    mutex_unlock(&cn->lock);
}

static struct channel_msg *channel_pop_rcvmsg(struct channel *cn) {
    struct channel_msg_item *msgi = NULL;
    struct channel_msg *msg = NULL;

    if (!list_empty(&cn->rcv_head)) {
	msgi = list_first(&cn->rcv_head, struct channel_msg_item, item);
	list_del_init(&msgi->item);
	msg = &msgi->msg;
    }
    return msg;
}


static int channel_push_sndmsg(struct channel *cn, struct channel_msg *msg) {
    int rc = 0;
    struct channel_msg_item *msgi = pio_cont(msg, struct channel_msg_item, msg);
    list_add_tail(&msgi->item, &cn->snd_head);
    return rc;
}

static struct channel_msg *channel_pop_sndmsg(struct channel *cn) {
    struct channel_msg_item *msgi;
    struct channel_msg *msg = NULL;
    
    mutex_lock(&cn->lock);
    if (!list_empty(&cn->snd_head)) {
	msgi = list_first(&cn->snd_head, struct channel_msg_item, item);
	list_del_init(&msgi->item);
	msg = &msgi->msg;

	/* Wakeup the blocking waiters. fix by needed here */
	condition_broadcast(&cn->cond);
    }
    mutex_unlock(&cn->lock);

    return msg;
}

int channel_recv(int cd, struct channel_msg **msg) {
    int rc = 0;
    struct channel *cn = global_channel(cd);

    mutex_lock(&cn->lock);
    while (!(*msg = channel_pop_rcvmsg(cn)) && !cn->fasync)
	condition_wait(&cn->cond, &cn->lock);
    mutex_unlock(&cn->lock);
    if (!*msg)
	rc = -EAGAIN;
    return rc;
}

int channel_send(int cd, struct channel_msg *msg) {
    int rc = 0;
    struct channel *cn = global_channel(cd);

    mutex_lock(&cn->lock);
    while ((rc = channel_push_sndmsg(cn, msg)) < 0 && rc != -EAGAIN)
	condition_wait(&cn->cond, &cn->lock);
    mutex_unlock(&cn->lock);
    return rc;
}


static int msg_ready(struct bio *b, int64_t *payload_sz, int64_t *control_sz) {
    struct channel_msg_item msgi = {};
    
    if (b->bsize < sizeof(msgi.hdr))
	return false;
    bio_copy(b, (char *)(&msgi.hdr), sizeof(msgi.hdr));
    if (b->bsize < channel_msgiov_len(&msgi.msg))
	return false;
    *payload_sz = msgi.hdr.payload_sz;
    *control_sz = msgi.hdr.control_sz;
    return true;
}

static int accepter_event_handler(epoll_t *el, epollevent_t *et) {
    int rc = 0;
    return rc;
}

static int io_event_handler(epoll_t *el, epollevent_t *et) {
    int rc = 0;
    struct channel *cn = pio_cont(et, struct channel, et);
    struct channel_msg *msg;
    int64_t payload_sz = 0, control_sz = 0;

    if (et->events & EPOLLIN) {
	if ((rc = bio_prefetch(&cn->in, &cn->sock_ops)) < 0 && errno != EAGAIN)
	    goto EXIT;
	while (msg_ready(&cn->in, &payload_sz, &control_sz)) {
	    msg = channel_allocmsg(payload_sz, control_sz);
	    bio_read(&cn->in, channel_msgiov_base(msg), channel_msgiov_len(msg));
	    channel_push_rcvmsg(cn, msg);
	}
    }
    if (et->events & EPOLLOUT) {
	while ((msg = channel_pop_sndmsg(cn)) != NULL) {
	    bio_write(&cn->out, channel_msgiov_base(msg), channel_msgiov_len(msg));
	    channel_freemsg(msg);
	}
	if ((rc = bio_flush(&cn->out, &cn->sock_ops)) < 0 && errno != EAGAIN)
	    goto EXIT;
    }
    if ((rc < 0 && errno != EAGAIN) || et->events & (EPOLLERR|EPOLLRDHUP))
	cn->fok = false;
    global_channel_poll_state(cn);
 EXIT:
    return rc;
}




