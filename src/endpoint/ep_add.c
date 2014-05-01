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
#include <os/alloc.h>
#include <xio/socket.h>
#include "ep_struct.h"


int xep_add(int eid, int sockfd) {
    struct endpoint *ep = eid_get(eid);
    int fnb = 1;
    int socktype = XCONNECTOR;
    int optlen = sizeof(socktype);
    struct endsock *s;
    struct list_head *head;

    if (!(socktype & (XCONNECTOR|XLISTENER))) {
	errno = EBADF;
	return -1;
    }
    if (!(s = (struct endsock *)mem_zalloc(sizeof(*s))))
	return -1;
    s->owner = ep;
    s->sockfd = sockfd;
    xsetsockopt(sockfd, XL_SOCKET, XNOBLOCK, &fnb, sizeof(fnb));
    xgetsockopt(sockfd, XL_SOCKET, XSOCKTYPE, &socktype, &optlen);
    head = socktype == XCONNECTOR ? &ep->csocks : &ep->bsocks;
    list_add_tail(&s->link, head);
    if (ep->type == XEP_PRODUCER)
	uuid_generate(s->uuid);
    DEBUG_OFF("endpoint %d add %d socket", eid, sockfd);
    return 0;
}
