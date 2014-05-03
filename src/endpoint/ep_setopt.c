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

typedef int (*ep_setopt) (struct endpoint *ep, void *val, int len);


static int start_proxy(struct endpoint *ep, int backend_eid) {
    struct proxy *py;
    struct endpoint *backend;

    if (backend_eid < 0) {
	errno = EINVAL;
	return -1;
    }
    backend = eid_get(backend_eid);
    if (ep->type != XEP_COMSUMER || backend->type != XEP_PRODUCER) {
	errno = EINVAL;
	return -1;
    }
    if (!(py = proxy_open(ep2eid(ep), backend_eid)))
	return -1;
    return 0;
}

static int stop_proxy(struct endpoint *ep) {
    struct proxy *py = ep->owner;

    proxy_close(py);
    return 0;
}

static int set_dispatchto(struct endpoint *ep, void *val, int len) {
    int rc;
    int backend_eid = *(int *)val;

    rc = ep->owner ? stop_proxy(ep) : start_proxy(ep, backend_eid);
    return rc;
}


static const ep_setopt setopt_vfptr[] = {
    0,
    set_dispatchto,
};


int xep_setopt(int eid, int opt, void *optval, int optlen) {
    int rc;
    struct endpoint *ep = eid_get(eid);

    if (opt < 0 || opt >= NELEM(setopt_vfptr, ep_setopt) || !setopt_vfptr[opt]) {
	errno = EINVAL;
	return -1;
    }
    rc = setopt_vfptr[opt] (ep, optval, optlen);
    return rc;
}
