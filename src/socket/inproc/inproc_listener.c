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
#include "../global.h"

/* sock's proc field operation.
 */

struct sockbase *getlistener (const char *addr) {
	int refed = false;
	struct str_rbe *entry;
	struct sockbase *sb = 0;

	xglobal_lock();
	if ((entry = str_rb_find (&xgb.inproc_listeners, addr, strlen (addr)))) {
		sb = & (cont_of (entry, struct inproc_sock, lhentry) )->base;
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

static int addlistener (struct str_rbe *entry)
{
	int rc = -1;

	xglobal_lock();
	if (!str_rb_find (&xgb.inproc_listeners, entry->key, entry->keylen) ) {
		rc = 0;
		DEBUG_OFF ("add listener %s", entry->key);
		str_rb_insert (&xgb.inproc_listeners, entry);
	}
	xglobal_unlock();
	return rc;
}


static void rmlistener (struct str_rbe *entry)
{
	xglobal_lock();
	str_rb_delete (&xgb.inproc_listeners, entry);
	xglobal_unlock();
}

/* sock_inproc_spec
 */

static struct sockbase *inproc_alloc() {
	struct inproc_sock *self = TNEW (struct inproc_sock);

	if (self) {
		sockbase_init (&self->base);
		return &self->base;
	}
	return 0;
}

static int inproc_listener_bind (struct sockbase *sb, const char *sock)
{
	struct str_rbe *entry = 0;
	struct inproc_sock *self = cont_of (sb, struct inproc_sock, base);

	strcpy (sb->addr, sock);
	entry = &self->lhentry;
	entry->key = sb->addr;
	entry->keylen = strlen (sb->addr);
	if (addlistener (entry)) {
		errno = EADDRINUSE;
		return -1;
	}
	return 0;
}

static void inproc_listener_close (struct sockbase *sb)
{
	struct sockbase *nsb;
	struct inproc_sock *self = cont_of (sb, struct inproc_sock, base);

	/* Avoiding the new connectors and remove listen file */
	rmlistener (&self->lhentry);

	/* we remove the file here because of Unix domain socket doesn't unlink
	 * the file when socket closed. and don't support getsockname ()
	 * or getpeername () API. */
	remove (sb->addr);

	/* Destroy acceptq's connection */
	while (acceptq_rm_nohup (sb, &nsb) == 0) {
		DEBUG_OFF ("listener %d close unaccept socket %d", sb->fd, nsb->fd);
		xclose (nsb->fd);
	}

	/* Close the sock and free sock id. */
	sockbase_exit (sb);
	mem_free (self, sizeof (*self) );
}

struct sockbase_vfptr inproc_listener_spec = {
	.type = XLISTENER,
	.pf = TP_INPROC,
	.alloc = inproc_alloc,
	.send = 0,
	.bind = inproc_listener_bind,
	.close = inproc_listener_close,
	.getopt = 0,
	.setopt = 0,
};
