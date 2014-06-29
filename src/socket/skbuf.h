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

#ifndef _H_PROXYIO_SKBUF_
#define _H_PROXYIO_SKBUF_

#include <utils/base.h>
#include <utils/list.h>
#include <xio/socket.h>
#include <xio/cmsghdr.h>
#include <utils/atomic.h>

/* The transport protocol header is 6 bytes long and looks like this:
 * +--------+------------+------------+
 * | 0xffff | 0xffffffff | 0xffffffff |
 * +--------+------------+------------+
 * |  crc16 |    size    |   chunk    |
 * +--------+------------+------------+
 */


#define SKBUF_SUBNUMMARK 0xf           // 16
#define SKBUF_CMSGLENMARK 0xfff        // 4k

/* TODO: little endian and big endian */
struct skbuf {
	struct list_head item;
	struct list_head cmsg_head;
	atomic_t ref;

	struct {
		u16 checksum;
		u16 cmsg_num:4;
		u16 cmsg_length:12;
		u32 ubuf_len;
		char ubuf_base[0];
	} chunk;
};

u32 skbuf_len (struct skbuf *msg);
char *skbuf_base (struct skbuf *msg);
struct skbuf *skbuf_alloc (int size);
void skbuf_free (struct skbuf *msg);

#define walk_msg_s(pos, next, head)				\
    walk_each_entry_s(pos, next, head, struct skbuf, item)

int skbuf_serialize (struct skbuf *msg, struct list_head *head);




#endif
