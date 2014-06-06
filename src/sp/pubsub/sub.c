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

#include "sub.h"

static struct epbase *sub_ep_alloc() {
    struct sub_ep *sub_ep = (struct sub_ep *)mem_zalloc(sizeof(*sub_ep));

    if (sub_ep) {
	epbase_init(&sub_ep->base);
    }
    return &sub_ep->base;
}

static void sub_ep_destroy(struct epbase *ep) {
    struct sub_ep *sub_ep = cont_of(ep, struct sub_ep, base);
    epbase_exit(ep);
    mem_free(sub_ep, sizeof(*sub_ep));
}

static int sub_ep_add(struct epbase *ep, struct socktg *sk, char *ubuf) {
    return 0;
}

static int sub_ep_rm(struct epbase *ep, struct socktg *sk, char **ubuf) {
    return 0;
}

static int sub_ep_join(struct epbase *ep, struct socktg *sk, int nfd) {
    struct socktg *nsk = sp_generic_join(ep, nfd);

    if (!nsk)
	return -1;
    return 0;
}

static const ep_setopt setopt_vfptr[] = {
    0,
};

static const ep_getopt getopt_vfptr[] = {
    0,
};

static int sub_ep_setopt(struct epbase *ep, int opt, void *optval, int optlen) {
    int rc;
    if (opt < 0 || opt >= NELEM(setopt_vfptr, ep_setopt) || !setopt_vfptr[opt]) {
	errno = EINVAL;
	return -1;
    }
    rc = setopt_vfptr[opt] (ep, optval, optlen);
    return rc;
}

static int sub_ep_getopt(struct epbase *ep, int opt, void *optval, int *optlen) {
    int rc;
    if (opt < 0 || opt >= NELEM(getopt_vfptr, ep_getopt) || !getopt_vfptr[opt]) {
	errno = EINVAL;
	return -1;
    }
    rc = getopt_vfptr[opt] (ep, optval, optlen);
    return rc;
}

static struct epbase_vfptr sub_epbase = {
    .sp_family = SP_PUBSUB,
    .sp_type = SP_SUB,
    .alloc = sub_ep_alloc,
    .destroy = sub_ep_destroy,
    .add = sub_ep_add,
    .rm = sub_ep_rm,
    .join = sub_ep_join,
    .setopt = sub_ep_setopt,
    .getopt = sub_ep_getopt,
};

struct epbase_vfptr *sub_epbase_vfptr = &sub_epbase;






