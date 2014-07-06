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

struct msgbuf_head {
	int wnd;                    /* msgbuf windows                     */
	int size;                   /* current buffer size                */
	int waiters;                /* wait the empty or non-empty events */
	struct list_head head;      /* msgbuf head                        */
};

void msgbuf_head_init (struct msgbuf_head *bh, int wnd);

/* check the msgbuf_head is empty. return 1 if true, otherwise return 0 */
int msgbuf_head_empty (struct msgbuf_head *bh);

/* dequeue the first msgbuf from head. return 0 if head is non-empty and set
   ubuf correctly. otherwise return -1 (head is empty) */
int msgbuf_head_out (struct msgbuf_head *bh, char **ubuf);

/* enqueue the ubuf into head. return 0 if success, otherwise return -1
   (head is full) */
int msgbuf_head_in (struct msgbuf_head *bh, char *ubuf);


#endif
