#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "channel.h"
#include "transport/tcp/tcp.h"
#include "transport/ipc/ipc.h"

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
    struct channel *cn = container_of(io_ops, struct channel, io_ops);
    return cn->tp->read(cn->fd, buff, sz);
}

static int64_t channel_io_write(struct io *io_ops, char *buff, int64_t sz) {
    struct channel *cn = container_of(io_ops, struct channel, io_ops);
    return cn->tp->write(cn->fd, buff, sz);
}
