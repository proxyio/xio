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
#include <xio/socket.h>
#include <xio/poll.h>
#include "ep_struct.h"

typedef int (*ep_getopt) (struct endpoint *ep, void *val, int *len);

static int get_dispatchto(struct endpoint *ep, void *val, int *len) {
    struct xproxy *py = ep->owner;
    
    if (!py || ep->type != XEP_COMSUMER) {
	errno = EINVAL;
	return -1;
    }
    BUG_ON(!py->backend);
    *(int *)val = ep2eid(py->backend);
    return 0;
}


static const ep_getopt getopt_vfptr[] = {
    0,
    get_dispatchto,
};


int xep_getopt(int eid, int opt, void *optval, int *optlen) {
    int rc;
    struct endpoint *ep = eid_get(eid);

    if (opt < 0 || opt >= NELEM(getopt_vfptr, ep_getopt) || !getopt_vfptr[opt]) {
	errno = EINVAL;
	return -1;
    }
    rc = getopt_vfptr[opt] (ep, optval, optlen);
    return rc;
}
