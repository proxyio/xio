#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "sync/mutex.h"
#include "sync/spin.h"
#include "channel.h"

/*  Max number of concurrent channels. */
#define PIO_MAX_CHANNELS 10240

struct channel {
    spin_t lock;
    struct list_head rcv_head;
    struct list_head snd_head;

    uint32_t events;
    epollevent_t et;
    epoll_t *el;
    struct bio b;
    struct io sock_ops;
    int fd;
    struct transport *tp;
};


struct channel_global {
    mutex_t lock;
    
    /*  The global table of existing channel. The descriptor representing
        the channel is the index to this table. This pointer is also used to
        find out whether context is initialised. If it is NULL, context is
        uninitialised. */
    struct channel channels[PIO_MAX_CHANNELS];

    /*  Stack of unused file descriptors. */
    int unused[PIO_MAX_CHANNELS];

    /*  Number of actual open channels in the channel table. */
    size_t nchannels;

    /*  Combination of the flags listed above. */
    int flags;

    /*  List of all available transports. */
    struct list_head transport_head;
};

static struct channel_global cn_global = {};


#define list_for_each_transport(tp, head)			\
    list_for_each_entry(tp, head, struct transport, item)


static inline void add_transport(struct transport *tp) {
    tp->global_init();
    list_add(&tp->item, &cn_global.transport_head);
}

void channel_global_init() {
    int cd;

    mutex_init(&cn_global.lock);
    INIT_LIST_HEAD(&cn_global.transport_head);
    add_transport(tcp_transport);
    add_transport(ipc_transport);

    for (cd = 0; cd < PIO_MAX_CHANNELS; cd++)
	cn_global.unused[cd] = cd;
}

static int64_t channel_io_read(struct io *io_ops, char *buff, int64_t sz) {
    struct channel *cn = container_of(io_ops, struct channel, sock_ops);
    return cn->tp->read(cn->fd, buff, sz);
}

static int64_t channel_io_write(struct io *io_ops, char *buff, int64_t sz) {
    struct channel *cn = container_of(io_ops, struct channel, sock_ops);
    return cn->tp->write(cn->fd, buff, sz);
}

static struct io default_channel_ops = {
    .read = channel_io_read,
    .write = channel_io_write,
};

static void channel_init(struct channel *cn, int fd, struct transport *tp) {
    spin_init(&cn->lock);
    INIT_LIST_HEAD(&cn->rcv_head);
    INIT_LIST_HEAD(&cn->snd_head);
    bio_init(&cn->b);
    cn->sock_ops = default_channel_ops;
    cn->fd = fd;
    cn->tp = tp;
}

static void channel_destroy(struct channel *cn) {
    struct list_head head;
    struct channel_msg_item *item, *nx;

    cn->tp = NULL;
    cn->fd = -1;
    INIT_LIST_HEAD(&head);
    list_splice(&cn->rcv_head, &head);
    list_splice(&cn->snd_head, &head);
    list_for_each_channel_msg_safe(item, nx, &head)
	channel_freemsg(&item->msg);
    bio_destroy(&cn->b);
}


void channel_close(int cd) {
    struct channel *cn = &cn_global.channels[cd];
    struct transport *tp = cn->tp;

    spin_lock(&cn->lock);

    tp->close(cn->fd);
    channel_destroy(cn);

    mutex_lock(&cn_global.lock);

    /*  Remove this channel from the table, add it to unused channel
        table. */
    cn_global.unused[--cn_global.nchannels] = cd;

    mutex_unlock(&cn_global.lock);

    spin_unlock(&cn->lock);
}


int channel_listen(int pf, const char *addr) {
    int cd, s;
    struct channel *cn;
    struct transport *tp;

    mutex_lock(&cn_global.lock);
    list_for_each_transport(tp, &cn_global.transport_head)
	if (tp->proto == pf) {
	    if ((s = tp->bind(addr)) < 0)
		return s;

	    // Find a unused channel id and slot
	    cd = cn_global.unused[cn_global.nchannels++];
	    cn = &cn_global.channels[cd];

	    // Init channel from raw-level sockid and transport vfptr
	    channel_init(cn, s, tp);
	    return cd;
	}
    mutex_unlock(&cn_global.lock);
    return -EINVAL;
}

int channel_connect(int pf, const char *peer) {
    int cd, s;
    struct channel *cn;
    struct transport *tp;

    mutex_lock(&cn_global.lock);
    list_for_each_transport(tp, &cn_global.transport_head)
	if (tp->proto == pf) {
	    if ((s = tp->connect(peer)) < 0)
		return s;

	    // Find a unused channel id and slot
	    cd = cn_global.unused[cn_global.nchannels++];
	    cn = &cn_global.channels[cd];

	    // Init channel from raw-level sockid and transport vfptr
	    channel_init(cn, s, tp);
	    return cd;
	}
    mutex_unlock(&cn_global.lock);
    return -EINVAL;
}

int channel_accept(int cd) {
    int new_cd = -1, s;
    struct channel *new_cn;
    struct channel *cn = &cn_global.channels[cd];
    struct transport *tp = cn->tp;
    
    spin_lock(&cn->lock);
    if ((s = tp->accept(cn->fd)) < 0) {
	spin_unlock(&cn->lock);
	return s;
    }
    mutex_lock(&cn_global.lock);

    // Find a unused channel id and slot.
    new_cd = cn_global.unused[cn_global.nchannels++];
    new_cn = &cn_global.channels[new_cd];
    mutex_unlock(&cn_global.lock);
    spin_unlock(&cn->lock);

    // Init channel from raw-level sockid and transport vfptr.
    channel_init(new_cn, s, tp);
    return new_cd;
}

int channel_setopt(int cd, int opt, void *val, int valsz) {
    int rc;
    struct channel *cn = &cn_global.channels[cd];
    struct transport *tp = cn->tp;

    spin_lock(&cn->lock);

    rc = tp->setopt(cn->fd, opt, val, valsz);

    spin_unlock(&cn->lock);
    return rc;
}

int channel_getopt(int cd, int opt, void *val, int valsz) {
    int rc;
    struct channel *cn = &cn_global.channels[cd];
    struct transport *tp = cn->tp;

    spin_lock(&cn->lock);

    rc = tp->getopt(cn->fd, opt, val, valsz);

    spin_unlock(&cn->lock);
    return rc;
}


int channel_recv(int cd, struct channel_msg **msg) {
    struct channel *cn = &cn_global.channels[cd];
    struct channel_msg_item *msgi;

    spin_lock(&cn->lock);

    if (list_empty(&cn->rcv_head)) {
	spin_unlock(&cn->lock);
	return -EAGAIN;
    }
    msgi = list_first(&cn->rcv_head, struct channel_msg_item, item);
    *msg = &msgi->msg;
    list_del(&msgi->item);

    spin_unlock(&cn->lock);
    return 0;
}

int channel_send(int cd, const struct channel_msg *msg) {
    struct channel *cn = &cn_global.channels[cd];
    struct channel_msg_item *msgi;

    spin_lock(&cn->lock);

    msgi = container_of(msg, struct channel_msg_item, item);
    list_add_tail(&msgi->item, &cn->snd_head);

    spin_unlock(&cn->lock);
    return 0;
}





