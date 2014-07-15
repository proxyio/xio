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

#ifndef _H_PROXYIO_MSGBUF_HEAD_
#define _H_PROXYIO_MSGBUF_HEAD_

#include "msgbuf.h"


struct msgbuf_head;
typedef void (*msgbuf_head_event_hndl) (struct msgbuf_head *bh);

struct msgbuf_vfptr {
	void (*add)         (struct msgbuf_head *bh);
	void (*rm)          (struct msgbuf_head *bh);
	void (*empty)       (struct msgbuf_head *bh);
	void (*nonempty)    (struct msgbuf_head *bh);
	void (*full)        (struct msgbuf_head *bh);
	void (*nonfull)     (struct msgbuf_head *bh);
};

struct msgbuf_head {
	struct msgbuf_vfptr *ev_hndl;
	int wnd;                    /* msgbuf windows                     */
	int size;                   /* current buffer size                */
	int waiters;                /* wait the empty or non-empty events */
	struct list_head head;      /* msgbuf head                        */
};

void msgbuf_head_init (struct msgbuf_head *bh, int wnd);
void msgbuf_head_ev_hndl (struct msgbuf_head *bh, struct msgbuf_vfptr *ev_hndl);

/* we must guarantee that we can push one massage at least. */
static inline int msgbuf_can_in (struct msgbuf_head *bh)
{
	return (list_empty (&bh->head) || bh->size < bh->wnd);
}

/* check the msgbuf_head is empty. return 1 if true, otherwise return 0 */
int msgbuf_head_empty (struct msgbuf_head *bh);

/* dequeue the first msgbuf from head. return 0 if head is non-empty and set
   ubuf correctly. otherwise return -1 (head is empty) */
int msgbuf_head_out (struct msgbuf_head *bh, char **ubuf);

/* enqueue the ubuf into head. return 0 if success, otherwise return -1
   (head is full) */
int msgbuf_head_in (struct msgbuf_head *bh, char *ubuf);

static inline int msgbuf_head_out_msg (struct msgbuf_head *bh, struct msgbuf **msg)
{
	int rc;
	char *ubuf = 0;

	if ((rc = msgbuf_head_out (bh, &ubuf)) == 0) {
		*msg = get_msgbuf (ubuf);
	}
	return rc;
}

static inline int msgbuf_head_in_msg (struct msgbuf_head *bh, struct msgbuf *msg)
{
	return msgbuf_head_in (bh, get_ubuf (msg));
}


static inline void msgbuf_dequeue_all (struct msgbuf_head *bh, struct list_head *head)
{
	list_splice (&bh->head, head);
}

static inline int msgbuf_head_has_waiters (struct msgbuf_head *bh)
{
	return bh->waiters > 0;
}

static inline void msgbuf_head_incr_waiters (struct msgbuf_head *bh)
{
	bh->waiters++;
}

static inline void msgbuf_head_decr_waiters (struct msgbuf_head *bh)
{
	bh->waiters--;
}

#endif
