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
#include "msgbuf.h"


u32 msgbuf_len (struct msgbuf *msg)
{
	return sizeof (msg->chunk) + msg->chunk.ubuf_len;
}

u32 msgbuf_lens (struct msgbuf *msg)
{
	struct msgbuf *cmsg, *tmp;
	u32 iovlen = msgbuf_len (msg);

	walk_msg_s (cmsg, tmp, &msg->cmsg_head) {
		iovlen += msgbuf_lens (cmsg);
	}
	return iovlen;
}

char *msgbuf_base (struct msgbuf *msg)
{
	return (char *) &msg->chunk;
}

int msgbuf_serialize (struct msgbuf *msg, struct list_head *head)
{
	int rc = 0;
	struct msgbuf *cmsg, *tmp;

	rc++;
	list_add_tail (&msg->item, head);
	walk_msg_s (cmsg, tmp, &msg->cmsg_head) {
		list_del_init (&cmsg->item);
		rc += msgbuf_serialize (cmsg, head);
	}
	return rc;
}

struct msgbuf *msgbuf_alloc (int size) {
	struct msgbuf *msg = (struct msgbuf *) mem_zalloc (sizeof (*msg) + size);
	if (!msg)
		return 0;
	INIT_LIST_HEAD (&msg->item);
	INIT_LIST_HEAD (&msg->cmsg_head);
	msg->chunk.ubuf_len = size;
	msg->chunk.checksum = crc16 ((char *) &msg->chunk.ubuf_len, sizeof (msg->chunk.ubuf_len));
	return msg;
}

char *ubuf_alloc (int size)
{
	struct msgbuf *msg = msgbuf_alloc (size);
	if (!msg)
		return 0;
	return msg->chunk.ubuf_base;
}

void msgbuf_free (struct msgbuf *msg)
{
	struct msgbuf *cmsg, *tmp;
	walk_msg_s (cmsg, tmp, &msg->cmsg_head) {
		list_del_init (&cmsg->item);
		msgbuf_free (cmsg);
	}
	mem_free (msg, sizeof (*msg) + msg->chunk.ubuf_len);
}

void ubuf_free (char *ubuf)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);
	msgbuf_free (msg);
}

int ubuf_len (char *ubuf)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);
	return msg->chunk.ubuf_len;
}


typedef int (*msgbuf_ctl) (char *msgbuf, void *optval);

static int sub_msgbuf_num (char *ubuf, void *optval)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);
	* (int *) optval = msg->chunk.cmsg_num;
	return 0;
}

static int sub_msgbuf_first (char *ubuf, void *optval)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);
	struct msgbuf *cur;

	if (!list_empty (&msg->cmsg_head)) {
		cur = list_first (&msg->cmsg_head, struct msgbuf, item);
		* (char **) optval = cur->chunk.ubuf_base;
		return 0;
	}
	return -1;
}

static int sub_msgbuf_next (char *ubuf, void *optval)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);
	struct msgbuf *cur = cont_of (optval, struct msgbuf, chunk.ubuf_base);

	if (list_next (&cur->item) != &msg->cmsg_head) {
		cur = cont_of (list_next (&cur->item), struct msgbuf, chunk.ubuf_base);
		* (char **) optval = cur->chunk.ubuf_base;
		return 0;
	}
	return -1;
}

static int sub_msgbuf_tail (char *ubuf, void *optval)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);
	struct msgbuf *cur;

	if (!list_empty (&msg->cmsg_head)) {
		cur = list_last (&msg->cmsg_head, struct msgbuf, item);
		* (char **) optval = cur->chunk.ubuf_base;
		return 0;
	}
	return -1;
}

static int sub_msgbuf_add (char *ubuf, void *optval)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);
	struct msgbuf *new = cont_of (optval, struct msgbuf, chunk.ubuf_base);

	if (msg->chunk.cmsg_num == MSGBUF_SUBNUMMARK ||
	    msg->chunk.cmsg_length + msgbuf_lens (new) > MSGBUF_CMSGLENMARK) {
		errno = EFBIG;
		return -1;
	}
	msg->chunk.cmsg_num++;
	msg->chunk.cmsg_length += msgbuf_lens (new);
	list_add_tail (&new->item, &msg->cmsg_head);
	return 0;
}

static int sub_msgbuf_rm (char *ubuf, void *optval)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);
	struct msgbuf *rm = cont_of (optval, struct msgbuf, chunk.ubuf_base);

	msg->chunk.cmsg_num--;
	msg->chunk.cmsg_length -= msgbuf_lens (rm);
	list_del_init (&rm->item);
	BUG_ON(msg->chunk.cmsg_num < 0 || msg->chunk.cmsg_length < 0);
	return 0;
}

static int sub_msgbuf_switch (char *ubuf, void *optval)
{
	struct msgbuf *src = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);
	struct msgbuf *dst = cont_of (optval, struct msgbuf, chunk.ubuf_base);

	dst->chunk.cmsg_num = src->chunk.cmsg_num;
	src->chunk.cmsg_num = 0;
	dst->chunk.cmsg_length = src->chunk.cmsg_length;
	src->chunk.cmsg_length = 0;
	list_splice (&src->cmsg_head, &dst->cmsg_head);
	return 0;
}

/* Simply copy the content of msgbuf to dest */
static char *ubufdup (char *ubuf) {
	char *dest = ubuf_alloc (ubuf_len (ubuf));

	if (dest)
		memcpy(dest, ubuf, ubuf_len (ubuf));
	return dest;
}

/* copy the msgbuf simply
   WARNING: if the msgbuf is high level tree, only the children of the msgbuf
   would be copied, the sub of children isn't, remember that. */
static int sub_msgbuf_copy (char *ubuf, void *optval) {
	struct msgbuf *src = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);
	struct msgbuf *cur, *tmp;
	char *dest = (char *) optval;

	walk_msg_s (cur, tmp, &src->cmsg_head) {
		sub_msgbuf_add (dest, ubufdup (cur->chunk.ubuf_base));
	}
	return 0;
}

static const msgbuf_ctl msgbuf_vfptr[] = {
	0,
	sub_msgbuf_num,       /* the number of sub msgbuf */
	sub_msgbuf_first,     /* the first sub msgbuf of this ubuf*/
	sub_msgbuf_next,      /* the next sub msgbuf specified by the second argument */
	sub_msgbuf_tail,      /* the last sub msgbuf of this ubuf */
	sub_msgbuf_add,       /* insert one msgbuf into children's head of the ubuf */
	sub_msgbuf_rm,        /* remove one msgbuf from children's head of the ubuf */
	sub_msgbuf_switch,    /* move the sub msgbuf from one to another */
	sub_msgbuf_copy,      /* copy a msgbuf include all the subs */
};


int ubufctl (char *ubuf, int opt, void *optval)
{
	int rc;

	if (opt < 0 || opt >= NELEM (msgbuf_vfptr, msgbuf_ctl)) {
		errno = EINVAL;
		return -1;
	}
	rc = msgbuf_vfptr[opt] (ubuf, optval);
	return rc;
}


int msgbuf_preinstall_iovs (struct msgbuf *msg, struct rex_iov *iovs, int n)
{
	int installed = 0;
	struct msgbuf *cmsg;

	BUG_ON (n < 0);
	if (n == 0)
		return 0;
	iovs->iov_len = msgbuf_len (msg) - msg->doff;
	iovs->iov_base = msgbuf_base (msg) + msg->doff;
	if (iovs->iov_len > 0)
		installed++;
	walk_msg (cmsg, &msg->cmsg_head) {
		installed += msgbuf_preinstall_iovs (cmsg, iovs + installed, n - installed);
		BUG_ON (installed > n);
		if (installed == n)
			break;
	}
	return installed;
}

static u32 __msgbuf_install_iovs (struct msgbuf *msg, struct rex_iov *iovs, u32 *length)
{
	u32 installed = 0;
	u32 diff;
	struct msgbuf *cmsg;

	diff = *length > (msgbuf_len (msg) - msg->doff) ? (msgbuf_len (msg) - msg->doff) : *length;
	msg->doff += diff;
	if ((*length -= diff) == 0)
		return msg->doff;
	BUG_ON (*length < 0);
	installed = msg->doff;

	walk_msg (cmsg, &msg->cmsg_head) {
		installed += __msgbuf_install_iovs (cmsg, iovs, length);
		if (*length == 0)
			break;
	}
	return installed;
}

int msgbuf_install_iovs (struct msgbuf *msg, struct rex_iov *iovs, u32 *length)
{
	if (__msgbuf_install_iovs (msg, iovs, length) == msgbuf_len (msg) + msg->chunk.cmsg_length)
		return 0;
	return -1;
}
