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

#ifndef _HPIO_XMSG_
#define _HPIO_XMSG_

#include <base.h>
#include <ds/list.h>
#include <xio/socket.h>
#include <xio/cmsghdr.h>

/* The transport protocol header is 6 bytes long and looks like this:
 * +--------+------------+------------+
 * | 0xffff | 0xffffffff | 0xffffffff |
 * +--------+------------+------------+
 * |  crc16 |    size    |   chunkdt  |
 * +--------+------------+------------+
 */


#define XMSG_CMSGNUMMARK 0xf           // 16
#define XMSG_CMSGLENMARK 0xfff         // 4k

/* TODO: little endian and big endian */
struct xiov {
    u16 checksum;
    u16 cmsg_num:4;
    u16 cmsg_length:12;
    u32 xiov_len;
    char xiov_base[0];
};

struct xmsg {
    struct list_head item;
    struct list_head cmsg_head;
    struct xiov vec;
};

u32 xmsg_iovlen(struct xmsg *msg);
char *xmsg_iovbase(struct xmsg *msg);
struct xmsg *xallocmsg(int size);
void xfreemsg(struct xmsg *msg);
int xmsglen(struct xmsg *msg);

#define xmsg_walk_safe(pos, next, head)					\
    list_for_each_entry_safe(pos, next, head, struct xmsg, item)

int xiov_serialize(struct xmsg *msg, struct list_head *head);




#endif
