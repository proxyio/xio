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

#include "msgbuf_head.h"

void msgbuf_head_init (struct msgbuf_head *bh, int wnd)
{
	bh->ev_hndl = 0;
	bh->wnd = wnd;
	bh->size = 0;
	bh->waiters = 0;
	INIT_LIST_HEAD (&bh->head);
}

void msgbuf_head_ev_hndl (struct msgbuf_head *bh, struct msgbuf_vfptr *ev_hndl)
{
	bh->ev_hndl = ev_hndl;
}

void msgbuf_head_walk (struct msgbuf_head *bh, walk_fn f, void *data)
{
	int rc;
	struct msgbuf *msg, *tmp;
	
	walk_msg_s (msg, tmp, &bh->head) {
		if ((rc = f (bh, msg, data)) != 0)
			break;
	}
}


int msgbuf_head_empty (struct msgbuf_head *bh)
{
	int rc;

	if ((rc = list_empty (&bh->head)))
		BUG_ON (bh->size != 0);
	return rc;
}

int msgbuf_head_out (struct msgbuf_head *bh, char **ubuf)
{
	struct msgbuf *msg = 0;

	if (msgbuf_head_empty (bh))
		return -1;
	msg = list_first (&bh->head, struct msgbuf, item);
	list_del_init (&msg->item);
	*ubuf = get_ubuf (msg);
	bh->size -= ulength (*ubuf);

	if (bh->ev_hndl && bh->ev_hndl->rm)
		bh->ev_hndl->rm (bh);

	/* the first time when msgbuf_head is non-full */
	if (bh->ev_hndl && bh->ev_hndl->nonfull && bh->wnd - bh->size > 0)
		bh->ev_hndl->nonfull (bh);
	if (bh->ev_hndl && bh->ev_hndl->empty && msgbuf_head_empty (bh))
		bh->ev_hndl->empty (bh);
	return 0;
}

int msgbuf_head_in (struct msgbuf_head *bh, char *ubuf)
{
	struct msgbuf *msg = get_msgbuf (ubuf);

	/* the first time when msgbuf_head is non-empty */
	if (bh->ev_hndl && bh->ev_hndl->nonempty && msgbuf_head_empty (bh))
		bh->ev_hndl->nonempty (bh);

	list_add_tail (&msg->item, &bh->head);
	bh->size += ulength (ubuf);

	if (bh->ev_hndl && bh->ev_hndl->add)
		bh->ev_hndl->add (bh);

	/* the first time when msgbuf_head is full */
	if (bh->ev_hndl && bh->ev_hndl->full && bh->wnd - bh->size <= 0)
		bh->ev_hndl->full (bh);
	return 0;
}

int msgbuf_head_preinstall_iovs (struct msgbuf_head *bh, struct rex_iov *iovs, int n)
{
	int installed = 0;
	struct msgbuf *msg;
	
	walk_msg (msg, &bh->head) {
		installed += msgbuf_preinstall_iovs (msg, iovs + installed, n - installed);
		BUG_ON (installed > n);
		if (installed == n)
			break;
	}
	return installed;
}


int msgbuf_head_install_iovs (struct msgbuf_head *bh, struct rex_iov *iovs, i64 length)
{
	int n = 0;
	struct msgbuf *msg;

	walk_msg (msg, &bh->head) {
		if (msgbuf_install_iovs (msg, iovs, &length) == 0)
			n++;
		BUG_ON (length < 0);
		if (length == 0)
			break;
	}
	return n;
}
