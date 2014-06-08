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


u32 skbuf_iovlen (struct skbuf *msg)
{
	return sizeof (msg->chunk) + msg->chunk.iov_len;
}

u32 skbuf_iovlens (struct skbuf *msg)
{
	struct skbuf *cmsg, *ncmsg;
	u32 iovlen = skbuf_iovlen (msg);

	walk_msg_s (cmsg, ncmsg, &msg->cmsg_head) {
		iovlen += skbuf_iovlens (cmsg);
	}
	return iovlen;
}

char *skbuf_iovbase (struct skbuf *msg)
{
	return (char *) &msg->chunk;
}

int skbuf_serialize (struct skbuf *msg, struct list_head *head)
{
	int rc = 0;
	struct skbuf *cmsg, *ncmsg;

	rc++;
	list_add_tail (&msg->item, head);
	walk_msg_s (cmsg, ncmsg, &msg->cmsg_head) {
		list_del_init (&cmsg->item);
		rc += skbuf_serialize (cmsg, head);
	}
	return rc;
}

struct skbuf *xallocmsg (int size) {
	struct skbuf *msg = (struct skbuf *) mem_zalloc (sizeof (*msg) + size);
	if (!msg)
		return 0;
	INIT_LIST_HEAD (&msg->item);
	INIT_LIST_HEAD (&msg->cmsg_head);
	msg->chunk.iov_len = size;
	msg->chunk.checksum = crc16 ( (char *) &msg->chunk.iov_len, sizeof (msg->chunk) -
	                              sizeof (u16) );
	return msg;
}

char *xallocubuf (int size)
{
	struct skbuf *msg = xallocmsg (size);
	if (!msg)
		return 0;
	return msg->chunk.iov_base;
}

void xfreemsg (struct skbuf *msg)
{
	struct skbuf *cmsg, *ncmsg;
	walk_msg_s (cmsg, ncmsg, &msg->cmsg_head) {
		list_del_init (&cmsg->item);
		xfreemsg (cmsg);
	}
	mem_free (msg, sizeof (*msg) + msg->chunk.iov_len);
}

void xfreeubuf (char *ubuf)
{
	struct skbuf *msg = cont_of (ubuf, struct skbuf, chunk.iov_base);
	xfreemsg (msg);
}

int skbuflen (struct skbuf *msg)
{
	return msg->chunk.iov_len;
}

int xubuflen (char *ubuf)
{
	struct skbuf *msg = cont_of (ubuf, struct skbuf, chunk.iov_base);
	return skbuflen (msg);
}


typedef int (*msgctl) (char *skbuf, void *optval);

static int subuf_num (char *ubuf, void *optval)
{
	struct skbuf *msg = cont_of (ubuf, struct skbuf, chunk.iov_base);
	* (int *) optval = msg->chunk.cmsg_num;
	return 0;
}

static int subuf_first (char *ubuf, void *optval)
{
	struct skbuf *msg = cont_of (ubuf, struct skbuf, chunk.iov_base);
	struct skbuf *cur;

	if (!list_empty (&msg->cmsg_head) ) {
		cur = list_first (&msg->cmsg_head, struct skbuf, item);
		* (char **) optval = cur->chunk.iov_base;
		return 0;
	}
	return -1;
}

static int subuf_next (char *ubuf, void *optval)
{
	struct skbuf *msg = cont_of (ubuf, struct skbuf, chunk.iov_base);
	struct skbuf *cur = cont_of (optval, struct skbuf, chunk.iov_base);

	if (list_next (&cur->item) != &msg->cmsg_head) {
		cur = cont_of (list_next (&cur->item), struct skbuf, chunk.iov_base);
		* (char **) optval = cur->chunk.iov_base;
		return 0;
	}
	return -1;
}

static int subuf_tail (char *ubuf, void *optval)
{
	struct skbuf *msg = cont_of (ubuf, struct skbuf, chunk.iov_base);
	struct skbuf *cur;

	if (!list_empty (&msg->cmsg_head) ) {
		cur = list_last (&msg->cmsg_head, struct skbuf, item);
		* (char **) optval = cur->chunk.iov_base;
		return 0;
	}
	return -1;
}

static int subuf_add (char *ubuf, void *optval)
{
	struct skbuf *msg = cont_of (ubuf, struct skbuf, chunk.iov_base);
	struct skbuf *new = cont_of (optval, struct skbuf, chunk.iov_base);

	if (msg->chunk.cmsg_num == SKBUF_SUBNUMMARK ||
	    msg->chunk.cmsg_length + skbuf_iovlens (new) > SKBUF_CMSGLENMARK) {
		errno = EFBIG;
		return -1;
	}
	msg->chunk.cmsg_num++;
	msg->chunk.cmsg_length += skbuf_iovlens (new);
	list_add_tail (&new->item, &msg->cmsg_head);
	return 0;
}

static int subuf_rm (char *ubuf, void *optval)
{
	struct skbuf *msg = cont_of (ubuf, struct skbuf, chunk.iov_base);
	struct skbuf *rm = cont_of (optval, struct skbuf, chunk.iov_base);

	msg->chunk.cmsg_num--;
	msg->chunk.cmsg_length -= skbuf_iovlens (rm);
	list_del_init (&rm->item);
	return 0;
}

static const msgctl msgctl_vfptr[] = {
	0,
	subuf_num,
	subuf_first,
	subuf_next,
	subuf_tail,
	subuf_add,
	subuf_rm,
};


int ubufctl (char *ubuf, int opt, void *optval)
{
	int rc;

	if (opt < 0 || opt >= NELEM (msgctl_vfptr, msgctl) ) {
		errno = EINVAL;
		return -1;
	}
	rc = msgctl_vfptr[opt] (ubuf, optval);
	return rc;
}
