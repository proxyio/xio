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
#include "../xgb.h"

/******************************************************************************
 *  xsock's proc field operation.
 ******************************************************************************/

struct sockbase *getlistener (const char *addr) {
	int refed = false;
	u32 size = strlen (addr);
	struct ssmap_node *node;
	struct sockbase *sb = 0;

	if (size > TP_SOCKADDRLEN)
		size = TP_SOCKADDRLEN;
	xglobal_lock();
	if ( (node = ssmap_find (&xgb.inproc_listeners, addr, size) ) ) {
		sb = & (cont_of (node, struct inproc_sock, rb_link) )->base;
		mutex_lock (&sb->lock);
		if (!sb->fepipe) {
			refed = true;
			atomic_incr (&sb->ref);
		}
		mutex_unlock (&sb->lock);
		if (!refed)
			sb = 0;
	}
	xglobal_unlock();
	return sb;
}

static int addlistener (struct ssmap_node *node)
{
	int rc = -1;

	xglobal_lock();
	if (!ssmap_find (&xgb.inproc_listeners, node->key, node->keylen) ) {
		rc = 0;
		DEBUG_OFF ("add listener %s", node->key);
		ssmap_insert (&xgb.inproc_listeners, node);
	}
	xglobal_unlock();
	return rc;
}


static void rmlistener (struct ssmap_node *node)
{
	xglobal_lock();
	ssmap_delete (&xgb.inproc_listeners, node);
	xglobal_unlock();
}

/******************************************************************************
 *  xsock_inproc_spec
 ******************************************************************************/

static struct sockbase *xinp_alloc() {
	struct inproc_sock *self = TNEW (struct inproc_sock);

	if (self) {
		xsock_init (&self->base);
		return &self->base;
	}
	return 0;
}

static int xinp_listener_bind (struct sockbase *sb, const char *sock)
{
	struct ssmap_node *node = 0;
	struct inproc_sock *self = cont_of (sb, struct inproc_sock, base);

	strncpy (sb->addr, sock, TP_SOCKADDRLEN);
	node = &self->rb_link;
	node->key = sb->addr;
	node->keylen = strlen (sb->addr);
	if (addlistener (node) ) {
		errno = EADDRINUSE;
		return -1;
	}
	return 0;
}

static void xinp_listener_close (struct sockbase *sb)
{
	struct sockbase *nsb;
	struct inproc_sock *self = cont_of (sb, struct inproc_sock, base);

	/* Avoiding the new connectors */
	rmlistener (&self->rb_link);

	/* Destroy acceptq's connection */
	while (acceptq_rm_nohup (sb, &nsb) == 0) {
		DEBUG_OFF ("listener %d close unaccept socket %d", sb->fd, nsb->fd);
		xclose (nsb->fd);
	}

	/* Close the xsock and free xsock id. */
	xsock_exit (sb);
	mem_free (self, sizeof (*self) );
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
