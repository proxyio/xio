#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "channel.h"

static struct list_head transport_head;

static inline void add_transport(struct transport *tp) {
    tp->global_init();
    list_add(&tp->item, &transport_head);
}

void channel_global_init() {
    INIT_LIST_HEAD(&transport_head);
    add_transport(tcp_transport);
    add_transport(ipc_transport);
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


void channel_init(struct channel *cn, int fd, struct transport *tp) {
    INIT_LIST_HEAD(&cn->rcv_head);
    INIT_LIST_HEAD(&cn->snd_head);
    bio_init(&cn->b);
    cn->sock_ops = default_channel_ops;
    cn->fd = fd;
    cn->tp = tp;
}


void channel_destroy(struct channel *cn) {
    struct list_head head;
    struct channel_msg_item *item, *nx;

    INIT_LIST_HEAD(&head);
    list_splice(&cn->rcv_head, &head);
    list_splice(&cn->snd_head, &head);
    list_for_each_channel_msg_safe(item, nx, &head)
	channel_freemsg(&item->msg);
    bio_destroy(&cn->b);
}


int channel_recv(struct channel *cn, struct channel_msg **msg) {
    struct channel_msg_item *msgi;

    if (list_empty(&cn->rcv_head)) {
	errno = EAGAIN;
	return -1;
    }
    msgi = list_first(&cn->rcv_head, struct channel_msg_item, item);
    *msg = &msgi->msg;
    list_del(&msgi->item);
    return 0;
}


int channel_send(struct channel *cn, const struct channel_msg *msg) {
    struct channel_msg_item *msgi;

    msgi = container_of(msg, struct channel_msg_item, item);
    list_add_tail(&msgi->item, &cn->snd_head);
    return 0;
}





