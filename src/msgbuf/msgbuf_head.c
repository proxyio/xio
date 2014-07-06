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
	bh->wnd = wnd;
	bh->size = 0;
	bh->waiters = 0;
	INIT_LIST_HEAD (&bh->head);
}

int msgbuf_head_empty (struct msgbuf_head *bh)
{
	int rc;

	BUG_ON (list_empty (&bh->head) && bh->size != 0);
	rc = list_empty (&bh->head);
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
	bh->size -= ubuf_len (*ubuf);
	return 0;
}

int msgbuf_head_in (struct msgbuf_head *bh, char *ubuf)
{
	struct msgbuf *msg = get_msgbuf (ubuf);

	list_add_tail (&msg->item, &bh->head);
	bh->size += ubuf_len(ubuf);
	return 0;
}

