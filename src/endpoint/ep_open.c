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
#include "ep_struct.h"

int xep_open(int type) {
    int eid;
    struct endpoint *ep;

    if (!(type & (XEP_PRODUCER|XEP_COMSUMER))) {
	errno = EINVAL;
	return -1;
    }
    if ((eid = eid_alloc()) < 0) {
	errno = EMFILE;
	return -1;
    }
    ep = eid_get(eid);
    ep->type = type;
    INIT_LIST_HEAD(&ep->bsocks);
    INIT_LIST_HEAD(&ep->csocks);
    INIT_LIST_HEAD(&ep->bad_socks);
    return eid;
}
