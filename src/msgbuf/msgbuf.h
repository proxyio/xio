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

#ifndef _H_PROXYIO_MSGBUF_
#define _H_PROXYIO_MSGBUF_

#include <utils/base.h>
#include <utils/list.h>
#include <xio/socket.h>
#include <xio/cmsghdr.h>
#include <utils/atomic.h>
#include <rex/rex.h>
#include <utils/bufio.h>
#include <utils/md5.h>


/* The transport protocol header is 6 bytes long and looks like this:
 * +--------+------------+------------+
 * | 0xffff | 0xffffffff | 0xffffffff |
 * +--------+------------+------------+
 * |  crc16 |    size    |   frame    |
 * +--------+------------+------------+
 */


#define MSGBUF_SUBNUMMARK 0xf           // 16
#define MSGBUF_CMSGLENMARK 0xfff        // 4k

struct msghdr {
	u16 checksum;
	u16 cmsg_num:4;
	u16 cmsg_length:12;
	u32 ulen;
	char ubase[0];
};


/* TODO: little endian and big endian */
struct msgbuf {
	struct list_head item;
	struct list_head cmsg_head;
	u32 doff;
	struct msghdr frame;
};

u32 msgbuf_len (struct msgbuf *msg);
char *msgbuf_base (struct msgbuf *msg);
struct msgbuf *msgbuf_alloc (int size);
void msgbuf_free (struct msgbuf *msg);

#define walk_msg(pos, head)				\
    walk_each_entry (pos, head, struct msgbuf, item)


#define walk_msg_s(pos, next, head)				\
    walk_each_entry_s(pos, next, head, struct msgbuf, item)

int msgbuf_serialize (struct msgbuf *msg, struct list_head *head);

static inline struct msgbuf *get_msgbuf (char *ubuf) {
	return cont_of (ubuf, struct msgbuf, frame.ubase);
}

static inline char *get_ubuf (struct msgbuf *msg)
{
	return msg->frame.ubase;
}


int msgbuf_preinstall_iovs (struct msgbuf *msg, struct rex_iov *iovs, int n);

int msgbuf_install_iovs (struct msgbuf *msg, struct rex_iov *iovs, i64 *length);

int msgbuf_deserialize (struct msgbuf **msg, struct bio *in);


void msgbuf_md5 (struct msgbuf *msg, unsigned char out[16]);



#endif
