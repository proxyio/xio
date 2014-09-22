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
	return sizeof (msg->frame) + msg->frame.ulen;
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
	return (char *) &msg->frame;
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
	msg->frame.ulen = size;
	msg->frame.checksum = crc16 ((char *) &msg->frame.ulen, sizeof (msg->frame.ulen));
	return msg;
}

char *ualloc (int size)
{
	struct msgbuf *msg = msgbuf_alloc (size);
	if (!msg)
		return 0;
	return msg->frame.ubase;
}

void msgbuf_free (struct msgbuf *msg)
{
	struct msgbuf *cmsg, *tmp;
	walk_msg_s (cmsg, tmp, &msg->cmsg_head) {
		list_del_init (&cmsg->item);
		msgbuf_free (cmsg);
	}
	mem_free (msg, sizeof (*msg) + msg->frame.ulen);
}

void ufree (char *ubuf)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, frame.ubase);
	msgbuf_free (msg);
}

int usize (char *ubuf)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, frame.ubase);
	return msg->frame.ulen;
}


typedef int (*msgbuf_ctl) (char *msgbuf, void *optval);

static int sub_msgbuf_num (char *ubuf, void *optval)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, frame.ubase);
	* (int *) optval = msg->frame.cmsg_num;
	return 0;
}

static int sub_msgbuf_first (char *ubuf, void *optval)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, frame.ubase);
	struct msgbuf *cur;

	if (!list_empty (&msg->cmsg_head)) {
		cur = list_first (&msg->cmsg_head, struct msgbuf, item);
		* (char **) optval = cur->frame.ubase;
		return 0;
	}
	return -1;
}

static int sub_msgbuf_next (char *ubuf, void *optval)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, frame.ubase);
	struct msgbuf *cur = cont_of (optval, struct msgbuf, frame.ubase);

	if (list_next (&cur->item) != &msg->cmsg_head) {
		cur = cont_of (list_next (&cur->item), struct msgbuf, frame.ubase);
		* (char **) optval = cur->frame.ubase;
		return 0;
	}
	return -1;
}

static int sub_msgbuf_tail (char *ubuf, void *optval)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, frame.ubase);
	struct msgbuf *cur;

	if (!list_empty (&msg->cmsg_head)) {
		cur = list_last (&msg->cmsg_head, struct msgbuf, item);
		* (char **) optval = cur->frame.ubase;
		return 0;
	}
	return -1;
}

static int sub_msgbuf_add (char *ubuf, void *optval)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, frame.ubase);
	struct msgbuf *new = cont_of (optval, struct msgbuf, frame.ubase);

	if (msg->frame.cmsg_num == MSGBUF_SUBNUMMARK ||
	    msg->frame.cmsg_length + msgbuf_lens (new) > MSGBUF_CMSGLENMARK) {
		errno = EFBIG;
		return -1;
	}
	msg->frame.cmsg_num++;
	msg->frame.cmsg_length += msgbuf_lens (new);
	list_add_tail (&new->item, &msg->cmsg_head);
	return 0;
}

static int sub_msgbuf_rm (char *ubuf, void *optval)
{
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, frame.ubase);
	struct msgbuf *rm = cont_of (optval, struct msgbuf, frame.ubase);

	msg->frame.cmsg_num--;
	msg->frame.cmsg_length -= msgbuf_lens (rm);
	list_del_init (&rm->item);
	BUG_ON(msg->frame.cmsg_num < 0 || msg->frame.cmsg_length < 0);
	return 0;
}

static int sub_msgbuf_switch (char *ubuf, void *optval)
{
	struct msgbuf *src = cont_of (ubuf, struct msgbuf, frame.ubase);
	struct msgbuf *dst = cont_of (optval, struct msgbuf, frame.ubase);

	dst->frame.cmsg_num = src->frame.cmsg_num;
	src->frame.cmsg_num = 0;
	dst->frame.cmsg_length = src->frame.cmsg_length;
	src->frame.cmsg_length = 0;
	list_splice (&src->cmsg_head, &dst->cmsg_head);
	return 0;
}

/* Simply copy the content of msgbuf to dest */
static char *ubufdup (char *ubuf) {
	char *dest = ualloc (usize (ubuf));

	if (dest)
		memcpy(dest, ubuf, usize (ubuf));
	return dest;
}

/* copy the msgbuf simply
   WARNING: if the msgbuf is high level tree, only the children of the msgbuf
   would be copied, the sub of children isn't, remember that. */
static int sub_msgbuf_copy (char *ubuf, void *optval) {
	struct msgbuf *src = cont_of (ubuf, struct msgbuf, frame.ubase);
	struct msgbuf *cur, *tmp;
	char *dest = (char *) optval;

	walk_msg_s (cur, tmp, &src->cmsg_head) {
		sub_msgbuf_add (dest, ubufdup (cur->frame.ubase));
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


int uctl (char *ubuf, int opt, void *optval)
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

static i64 __msgbuf_install_iovs (struct msgbuf *msg, struct rex_iov *iovs, i64 *length)
{
	i64 installed = 0;
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

int msgbuf_install_iovs (struct msgbuf *msg, struct rex_iov *iovs, i64 *length)
{
	if (__msgbuf_install_iovs (msg, iovs, length) == msgbuf_len (msg) + msg->frame.cmsg_length)
		return 0;
	return -1;
}

static int msgbuf_recv_finish (struct bio *b)
{
	struct msgbuf msg = {};

	if (b->bsize < sizeof (msg.frame))
		return false;
	bio_copy (b, (char *) (&msg.frame), sizeof (msg.frame));
	if (b->bsize < msgbuf_len (&msg) + (u32) msg.frame.cmsg_length)
		return false;
	return true;
}

static int extract_msgbuf (struct bio *b, struct msgbuf **msgp)
{
	i64 nbytes;
	struct msgbuf msg = {};

	bio_copy (b, (char *) &msg.frame, sizeof (msg.frame));
	*msgp = msgbuf_alloc (msg.frame.ulen);
	if (b->bsize < msgbuf_len (*msgp))
		return -1;
	nbytes = bio_read (b, msgbuf_base (*msgp), msgbuf_len (*msgp));
	BUG_ON (nbytes != msgbuf_len (*msgp));
	return 0;
}



int msgbuf_deserialize (struct msgbuf **msgp, struct bio *in)
{
	struct msgbuf *msg = 0, *s = 0;
	int rc;
	int sno;
	
	if (!msgbuf_recv_finish (in)) {
		errno = EAGAIN;
		return -1;
	}
	BUG_ON (extract_msgbuf (in, &msg) != 0);
	sno = msg->frame.cmsg_num;
	while (sno--) {
		BUG_ON ((rc = msgbuf_deserialize (&s, in)) != 0);
		list_add_tail (&s->item, &msg->cmsg_head);
	}
	*msgp = msg;
	return 0;
}


static void __msgbuf_md5 (struct msgbuf *msg, struct md5_state *md5)
{
	struct msgbuf *cmsg;

	md5_process (md5, msgbuf_base (msg), msgbuf_len (msg));
	walk_msg (cmsg, &msg->cmsg_head) {
		__msgbuf_md5 (cmsg, md5);
	}
}

void msgbuf_md5 (struct msgbuf *msg, unsigned char out[16])
{
	struct md5_state md5;

	md5_init (&md5);
	__msgbuf_md5 (msg, &md5);
	md5_done (&md5, out);
}
