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
#include <runner/taskpool.h>
#include "../xgb.h"

/******************************************************************************
 *  xsock's proc field operation.
 ******************************************************************************/

struct sockbase *find_listener(const char *addr) {
    u32 size = strlen(addr);
    struct ssmap_node *node;
    struct inproc_sock *self = 0;

    if (size > TP_SOCKADDRLEN)
	size = TP_SOCKADDRLEN;
    xglobal_lock();
    if ((node = ssmap_find(&xgb.inproc_listeners, addr, size)))
	self = cont_of(node, struct inproc_sock, rb_link);
    xglobal_unlock();
    return &self->base;
}

static int insert_listener(struct ssmap_node *node) {
    int rc = -1;

    xglobal_lock();
    if (!ssmap_find(&xgb.inproc_listeners, node->key, node->keylen)) {
	rc = 0;
	DEBUG_OFF("insert listener %s", node->key);
	ssmap_insert(&xgb.inproc_listeners, node);
    }
    xglobal_unlock();
    return rc;
}


static void remove_listener(struct ssmap_node *node) {
    xglobal_lock();
    ssmap_delete(&xgb.inproc_listeners, node);
    xglobal_unlock();
}

/******************************************************************************
 *  xsock_inproc_spec
 ******************************************************************************/

static struct sockbase *xinp_alloc() {
    struct inproc_sock *self = (struct inproc_sock *)mem_zalloc(sizeof(*self));

    if (self) {
	xsock_init(&self->base);
	return &self->base;
    }
    return 0;
}

static int xinp_listener_bind(struct sockbase *sb, const char *sock) {
    struct ssmap_node *node = 0;
    struct inproc_sock *self = cont_of(sb, struct inproc_sock, base);

    strncpy(sb->addr, sock, TP_SOCKADDRLEN);
    node = &self->rb_link;
    node->key = sb->addr;
    node->keylen = strlen(sb->addr);
    if (insert_listener(node)) {
	errno = EADDRINUSE;
	return -1;
    }
    return 0;
}

static void xinp_listener_close(struct sockbase *sb) {
    struct inproc_sock *self = cont_of(sb, struct inproc_sock, base);

    /* Avoiding the new connectors */
    remove_listener(&self->rb_link);

    /* Close the xsock and free xsock id. */
    xsock_exit(sb);
    mem_free(self, sizeof(*self));
}

struct sockbase_vfptr xinp_listener_spec = {
    .type = XLISTENER,
    .pf = XPF_INPROC,
    .alloc = xinp_alloc,
    .bind = xinp_listener_bind,
    .close = xinp_listener_close,
    .notify = 0,
    .getopt = 0,
    .setopt = 0,
};
