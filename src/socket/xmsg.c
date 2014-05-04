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
#include <os/alloc.h>
#include <hash/crc.h>
#include "xmsg.h"


u32 xiov_len(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    return sizeof(msg->vec) + msg->vec.size;
}

char *xiov_base(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    return (char *)&msg->vec;
}

char *xallocmsg(int size) {
    char *xbuf;
    struct xmsg *msg;

    char *chunk = (char *)mem_zalloc(sizeof(*msg) + size);
    if (!chunk)
	return 0;
    msg = (struct xmsg *)chunk;
    INIT_LIST_HEAD(&msg->item);
    INIT_LIST_HEAD(&msg->oob);

    msg->vec.size = size;
    msg->vec.checksum = crc16((char *)&msg->vec.size, 4);
    xbuf = msg->vec.chunk;
    DEBUG_OFF("%p", xbuf);
    return xbuf;
}

void xfreemsg(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    struct xmsg *oob, *nx_oob;

    DEBUG_OFF("%p", xbuf);
    xmsg_walk_safe(oob, nx_oob, &msg->oob) {
	list_del_init(&oob->item);
	xfreemsg(oob->vec.chunk);
    }
    mem_free(msg, sizeof(*msg) + msg->vec.size);
}

int xmsglen(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    return msg->vec.size;
}


typedef int (*msgctl) (char *xmsg, void *optval);

static int msgctl_countoob(char *xbuf, void *optval) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    *(int *)optval = msg->vec.oob;
    return 0;
}

static int msgctl_getoob(char *xbuf, void *optval) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    struct xmsgoob *ent = (struct xmsgoob *)optval;
    struct xmsg *oob, *nx_oob;

    if (!msg->vec.oob) {
	errno = ENOENT;
	return -1;
    }
    if (ent->pos < 0) {
	errno = EINVAL;
	return -1;
    }
    ent->pos = ent->pos > msg->vec.oob ? msg->vec.oob : ent->pos;
    xmsg_walk_safe(oob, nx_oob, &msg->oob) {
	if (-oob->vec.oob == ent->pos) {
	    ent->outofband = oob->vec.chunk;
	    return 0;
	}
    }
    BUG_ON(1);
    return -1;
}

static int msgctl_setoob(char *xbuf, void *optval) {
    struct xmsgoob *ent = (struct xmsgoob *)optval;
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    struct xmsg *new = cont_of(ent->outofband, struct xmsg, vec.chunk);

    if (new->vec.oob > 0 || ent->pos < 0) {
	errno = EINVAL;
	return -1;
    }
    msg->vec.oob++;
    new->vec.oob = -ent->pos;
    list_add_tail(&new->item, &msg->oob);
    return 0;
}

static const msgctl msgctl_vfptr[] = {
    msgctl_countoob,
    msgctl_getoob,
    msgctl_setoob,
};


int xmsgctl(char *xbuf, int opt, void *optval) {
    int rc;

    if (opt < 0 || opt >= NELEM(msgctl_vfptr, msgctl)) {
	errno = EINVAL;
	return -1;
    }
    rc = msgctl_vfptr[opt] (xbuf, optval);
    return rc;
}
