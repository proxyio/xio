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


u32 skbuf_len (struct skbuf *msg)
{
	return sizeof (msg->chunk) + msg->chunk.iov_len;
}

u32 skbuf_lens (struct skbuf *msg)
{
	struct skbuf *cmsg, *ncmsg;
	u32 iovlen = skbuf_len (msg);

	walk_msg_s (cmsg, ncmsg, &msg->cmsg_head) {
		iovlen += skbuf_lens (cmsg);
	}
	return iovlen;
}

char *skbuf_base (struct skbuf *msg)
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

struct skbuf *xalloc_skbuf (int size) {
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
	struct skbuf *msg = xalloc_skbuf (size);
	if (!msg)
		return 0;
	return msg->chunk.iov_base;
}

void xfree_skbuf (struct skbuf *msg)
{
	struct skbuf *cmsg, *ncmsg;
	walk_msg_s (cmsg, ncmsg, &msg->cmsg_head) {
		list_del_init (&cmsg->item);
		xfree_skbuf (cmsg);
	}
	mem_free (msg, sizeof (*msg) + msg->chunk.iov_len);
}

void xfreeubuf (char *ubuf)
{
	struct skbuf *msg = cont_of (ubuf, struct skbuf, chunk.iov_base);
	xfree_skbuf (msg);
}

int xubuflen (char *ubuf)
{
	struct skbuf *msg = cont_of (ubuf, struct skbuf, chunk.iov_base);
	return msg->chunk.iov_len;
}


typedef int (*skbuf_ctl) (char *skbuf, void *optval);

static int sub_skbuf_num (char *ubuf, void *optval)
{
	struct skbuf *msg = cont_of (ubuf, struct skbuf, chunk.iov_base);
	* (int *) optval = msg->chunk.cmsg_num;
	return 0;
}

static int sub_skbuf_first (char *ubuf, void *optval)
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

static int sub_skbuf_next (char *ubuf, void *optval)
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

static int sub_skbuf_tail (char *ubuf, void *optval)
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

static int sub_skbuf_add (char *ubuf, void *optval)
{
	struct skbuf *msg = cont_of (ubuf, struct skbuf, chunk.iov_base);
	struct skbuf *new = cont_of (optval, struct skbuf, chunk.iov_base);

	if (msg->chunk.cmsg_num == SKBUF_SUBNUMMARK ||
	    msg->chunk.cmsg_length + skbuf_lens (new) > SKBUF_CMSGLENMARK) {
		errno = EFBIG;
		return -1;
	}
	msg->chunk.cmsg_num++;
	msg->chunk.cmsg_length += skbuf_lens (new);
	list_add_tail (&new->item, &msg->cmsg_head);
	return 0;
}

static int sub_skbuf_rm (char *ubuf, void *optval)
{
	struct skbuf *msg = cont_of (ubuf, struct skbuf, chunk.iov_base);
	struct skbuf *rm = cont_of (optval, struct skbuf, chunk.iov_base);

	msg->chunk.cmsg_num--;
	msg->chunk.cmsg_length -= skbuf_lens (rm);
	list_del_init (&rm->item);
	BUG_ON(msg->chunk.cmsg_num < 0 || msg->chunk.cmsg_length < 0);
	return 0;
}

static const skbuf_ctl skbuf_vfptr[] = {
	0,
	sub_skbuf_num,       /* the number of sub skbuf */
	sub_skbuf_first,     /* the first sub skbuf of this ubuf*/
	sub_skbuf_next,      /* the next sub skbuf specified by the second argument */
	sub_skbuf_tail,      /* the last sub skbuf of this ubuf */
	sub_skbuf_add,       /* insert one skbuf into children's head of the ubuf */
	sub_skbuf_rm,        /* remove one skbuf from children's head of the ubuf */
};


int ubufctl (char *ubuf, int opt, void *optval)
{
	int rc;

	if (opt < 0 || opt >= NELEM (skbuf_vfptr, skbuf_ctl) ) {
		errno = EINVAL;
		return -1;
	}
	rc = skbuf_vfptr[opt] (ubuf, optval);
	return rc;
}
