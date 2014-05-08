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
#include "xgb.h"

/******************************************************************************
 *  xsock's proc field operation.
 ******************************************************************************/

struct xsock *find_listener(const char *addr) {
    struct ssmap_node *node;
    struct xsock *self = 0;
    u32 size = strlen(addr);

    if (size > TP_SOCKADDRLEN)
	size = TP_SOCKADDRLEN;
    xglobal_lock();
    if ((node = ssmap_find(&xgb.inproc_listeners, addr, size)))
	self = cont_of(node, struct xsock, proc.rb_link);
    xglobal_unlock();
    return self;
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

static int xinp_listener_bind(int fd, const char *sock) {
    int rc;
    struct ssmap_node *node = 0;
    struct xsock *self = xget(fd);

    ZERO(self->proc);
    strncpy(self->addr, sock, TP_SOCKADDRLEN);

    node = &self->proc.rb_link;
    node->key = self->addr;
    node->keylen = strlen(self->addr);
    if ((rc = insert_listener(node)) < 0) {
	errno = EADDRINUSE;
	return -1;
    }
    return 0;
}

static void xinp_listener_close(int fd) {
    struct xsock *self = xget(fd);

    /* Avoiding the new connectors */
    remove_listener(&self->proc.rb_link);

    /* Close the xsock and free xsock id. */
    xsock_free(self);
}

struct sockbase_vfptr xinp_listener_spec = {
    .type = XLISTENER,
    .pf = XPF_INPROC,
    .bind = xinp_listener_bind,
    .close = xinp_listener_close,
    .notify = 0,
    .getopt = 0,
    .setopt = 0,
};
