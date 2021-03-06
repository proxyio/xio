/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <utils/taskpool.h>
#include "inproc.h"

extern struct sockbase* getlistener(const char* addr);

static struct sockbase* inproc_open() {
    struct sockbase* sb;
    struct inproc_sock* self = mem_zalloc(sizeof(struct inproc_sock));

    if (!self) {
        return 0;
    }

    sockbase_init(&self->base);
    atomic_init(&self->ref);
    atomic_incr(&self->ref);
    sb = &self->base;
    return sb;
}

static int inproc_send(struct sockbase* sb, char* ubuf) {
    int rc;
    struct sockbase* peer = cont_of(sb, struct inproc_sock, base)->peer;

    rc = rcv_msgbuf_head_add(peer, get_msgbuf(ubuf));
    return rc;
}

static int inproc_connector_bind(struct sockbase* sb, const char* sock) {
    int new_fd = 0;
    struct sockbase* new_sb = 0;
    struct sockbase* listener = getlistener(sock);
    struct inproc_sock* self = 0;
    struct inproc_sock* peer = 0;

    if (!listener) {
        errno = ECONNREFUSED;
        return -1;
    }

    if ((new_fd = xalloc(sb->vfptr->pf, sb->vfptr->type)) < 0) {
        errno = EMFILE;
        xput(listener->fd);
        return -1;
    }

    new_sb = xgb.sockbases[new_fd];
    self = cont_of(sb, struct inproc_sock, base);
    peer = cont_of(new_sb, struct inproc_sock, base);
    new_sb->vfptr = sb->vfptr;
    strcpy(sb->peer, sock);
    strcpy(new_sb->addr, sock);

    atomic_incr(&self->ref);
    atomic_incr(&peer->ref);
    self->peer = &peer->base;
    peer->peer = &self->base;

    if (acceptq_add(listener, new_sb) < 0) {
        atomic_decr(&peer->ref);
        sb->vfptr->close(new_sb);
        xput(listener->fd);
        errno = ECONNREFUSED;
        return -1;
    }

    xput(listener->fd);
    SKLOG_DEBUG(sb, "%d accept new connection %d", sb->fd, new_fd);
    return 0;
}

static void inproc_peer_close(struct inproc_sock* peer) {
    /* Destroy the sock and free sock id if i hold the last ref. */
    mutex_lock(&peer->base.lock);

    if (peer->base.rcv.waiters || peer->base.snd.waiters) {
        condition_broadcast(&peer->base.cond);
    }

    mutex_unlock(&peer->base.lock);

    if (atomic_decr(&peer->ref) == 1) {
        sockbase_exit(&peer->base);
        atomic_destroy(&peer->ref);
        mem_free(peer, sizeof(*peer));
    }
}

static void inproc_connector_close(struct sockbase* sb) {
    struct inproc_sock* self = cont_of(sb, struct inproc_sock, base);
    struct inproc_sock* peer = 0;

    /* TODO: bug on here ??? */
    if (self->peer) {
        peer = cont_of(self->peer, struct inproc_sock, base);
        inproc_peer_close(peer);
    }

    if (atomic_decr(&self->ref) == 1) {
        sockbase_exit(&self->base);
        atomic_destroy(&self->ref);
        mem_free(self, sizeof(*self));
    }
}

extern int xgeneric_recv(struct sockbase* sb, char** ubuf);
extern int inproc_getopt(struct sockbase* sb, int opt, void* optval, int* optlen);
extern int inproc_setopt(struct sockbase* sb, int opt, void* optval, int optlen);

struct sockbase_vfptr inproc_connector_vfptr = {
    .type = XCONNECTOR,
    .pf = XAF_INPROC,
    .open = inproc_open,
    .getopt = inproc_getopt,
    .setopt = inproc_setopt,
    .send = inproc_send,
    .recv = xgeneric_recv,
    .bind = inproc_connector_bind,
    .close = inproc_connector_close,
};
