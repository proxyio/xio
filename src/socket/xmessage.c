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
#include <utils/alloc.h>
#include <utils/crc.h>
#include "xmessage.h"


u32 xmsg_iovlen(struct xmsg *msg) {
    return sizeof(msg->vec) + msg->vec.xiov_len;
}

u32 xmsg_iovlens(struct xmsg *msg) {
    struct xmsg *cmsg, *ncmsg;
    u32 iovlen = xmsg_iovlen(msg);

    xmsg_walk_safe(cmsg, ncmsg, &msg->cmsg_head) {
	iovlen += xmsg_iovlens(cmsg);
    }
    return iovlen;
}

char *xmsg_iovbase(struct xmsg *msg) {
    return (char *)&msg->vec;
}

int xmsg_serialize(struct xmsg *msg, struct list_head *head) {
    int rc = 0;
    struct xmsg *cmsg, *ncmsg;

    rc++;
    list_add_tail(&msg->item, head);
    xmsg_walk_safe(cmsg, ncmsg, &msg->cmsg_head) {
	list_del_init(&cmsg->item);
	rc += xmsg_serialize(cmsg, head);
    }
    return rc;
}

struct xmsg *xallocmsg(int size) {
    struct xmsg *msg = (struct xmsg *)mem_zalloc(sizeof(*msg) + size);
    if (!msg)
	return 0;
    INIT_LIST_HEAD(&msg->item);
    INIT_LIST_HEAD(&msg->cmsg_head);
    msg->vec.xiov_len = size;
    msg->vec.checksum = crc16((char *)&msg->vec.xiov_len, sizeof(msg->vec) -
			      sizeof(u16));
    return msg;
}

char *xallocubuf(int size) {
    struct xmsg *msg = xallocmsg(size);
    if (!msg)
	return 0;
    return msg->vec.xiov_base;
}

void xfreemsg(struct xmsg *msg) {
    struct xmsg *cmsg, *ncmsg;
    xmsg_walk_safe(cmsg, ncmsg, &msg->cmsg_head) {
	list_del_init(&cmsg->item);
	xfreemsg(cmsg);
    }
    mem_free(msg, sizeof(*msg) + msg->vec.xiov_len);
}

void xfreeubuf(char *ubuf) {
    struct xmsg *msg = cont_of(ubuf, struct xmsg, vec.xiov_base);
    xfreemsg(msg);
}

int xmsglen(struct xmsg *msg) {
    return msg->vec.xiov_len;
}

int xubuflen(char *ubuf) {
    struct xmsg *msg = cont_of(ubuf, struct xmsg, vec.xiov_base);
    return xmsglen(msg);
}



typedef int (*msgctl) (char *xmsg, void *optval);

static int msgctl_cmsgnum(char *ubuf, void *optval) {
    struct xmsg *msg = cont_of(ubuf, struct xmsg, vec.xiov_base);
    *(int *)optval = msg->vec.cmsg_num;
    return 0;
}

static int msgctl_getcmsg(char *ubuf, void *optval) {
    struct xmsg *msg = cont_of(ubuf, struct xmsg, vec.xiov_base);
    struct xcmsg *ent = (struct xcmsg *)optval;
    int pos;
    struct xmsg *cmsg, *ncmsg;

    if ((pos = ent->idx) >= msg->vec.cmsg_num) {
	errno = ENOENT;
	return -1;
    }
    xmsg_walk_safe(cmsg, ncmsg, &msg->cmsg_head) {
	if (pos--)
	    continue;
	ent->outofband = cmsg->vec.xiov_base;
	return 0;
    }
    BUG_ON(1);
    return -1;
}

static int msgctl_addcmsg(char *ubuf, void *optval) {
    struct xcmsg *ent = (struct xcmsg *)optval;
    struct xmsg *msg = cont_of(ubuf, struct xmsg, vec.xiov_base);
    struct xmsg *new = cont_of(ent->outofband, struct xmsg, vec.xiov_base);

    if (msg->vec.cmsg_num == XMSG_CMSGNUMMARK ||
	msg->vec.cmsg_length + xmsg_iovlens(new) > XMSG_CMSGLENMARK) {
	errno = EFBIG;
	return -1;
    }
    msg->vec.cmsg_num++;
    msg->vec.cmsg_length += xmsg_iovlens(new);
    list_add_tail(&new->item, &msg->cmsg_head);
    return 0;
}

static int msgctl_rmcmsg(char *ubuf, void *optval) {
    int rc;
    struct xcmsg *ent = (struct xcmsg *)optval;
    struct xmsg *rm;
    struct xmsg *msg = cont_of(ubuf, struct xmsg, vec.xiov_base);

    if ((rc = msgctl_getcmsg(ubuf, optval)))
	return rc;
    rm = cont_of(ent->outofband, struct xmsg, vec.xiov_base);
    msg->vec.cmsg_num--;
    msg->vec.cmsg_length -= xmsg_iovlens(rm);
    list_del_init(&rm->item);
    return 0;
}

static const msgctl msgctl_vfptr[] = {
    msgctl_cmsgnum,
    msgctl_getcmsg,
    msgctl_addcmsg,
    msgctl_rmcmsg,
};


int xmsgctl(char *ubuf, int opt, void *optval) {
    int rc;

    if (opt < 0 || opt >= NELEM(msgctl_vfptr, msgctl)) {
	errno = EINVAL;
	return -1;
    }
    rc = msgctl_vfptr[opt] (ubuf, optval);
    return rc;
}
