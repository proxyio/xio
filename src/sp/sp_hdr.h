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

#ifndef _XIO_SPHDR_
#define _XIO_SPHDR_

#include <errno.h>
#include <uuid/uuid.h>
#include <utils/list.h>
#include <utils/timer.h>
#include <utils/crc.h>
#include <xio/socket.h>
#include <xio/cmsghdr.h>


/* The sphdr looks like this:
 * +-------+----------------------+-------+--------+
 * | sphdr |  protocol specified  |  uhdr |  ubuf  |
 * +-------+----------------------+-------+--------+
 */

struct sphdr {
    u8 protocol;                    /* scalability protocol type    */
    u8 version;                     /* scalability protocol version */
    u16 timeout;                    /* message timeout by ms */
    i64 sendstamp;                  /* the timestamp when massage first send */
    union {
        u64 __align[2];
        struct list_head link;
    } u;
};

/* scalability protocol's header is saved in cmsg_head of ubuf and must be
   the first of it */
static inline struct sphdr *get_sphdr(char *ubuf) {
    return (struct sphdr *)ubufctl_first(ubuf);
}

/* checking the message is timeout or not, if timeout, the action was specified
   by the scalability protocols. for example, drop it simply. */
static inline int sphdr_timeout(struct sphdr *sh)
{
    return sh->timeout && (sh->sendstamp + sh->timeout < gettimeof(ms));
}

#endif
