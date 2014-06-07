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

#include "pub.h"

static struct epbase *pub_ep_alloc() {
    struct pub_ep *pub_ep = TNEW(struct pub_ep);

    if (pub_ep) {
        epbase_init(&pub_ep->base);
    }
    return &pub_ep->base;
}

static void pub_ep_destroy(struct epbase *ep)
{
    struct pub_ep *pub_ep = cont_of(ep, struct pub_ep, base);
    epbase_exit(ep);
    mem_free(pub_ep, sizeof(*pub_ep));
}

static int pub_ep_send(struct epbase *ep, char *ubuf)
{
    int rc = 0;
    return rc;
}

static int pub_ep_add(struct epbase *ep, struct tgtd *tg, char *ubuf)
{
    int rc = 0;
    return rc;
}

static int pub_ep_rm(struct epbase *ep, struct tgtd *tg, char **ubuf)
{
    int rc = 0;
    return rc;
}

static int pub_ep_join(struct epbase *ep, struct tgtd *tg, int nfd)
{
    struct tgtd *_tg = sp_generic_join(ep, nfd);

    if (!_tg)
        return -1;
    uuid_generate(_tg->uuid);
    return 0;
}

static const ep_setopt setopt_vfptr[] = {
    0,
};

static const ep_getopt getopt_vfptr[] = {
    0,
};

static int pub_ep_setopt(struct epbase *ep, int opt, void *optval, int optlen)
{
    int rc;
    if (opt < 0 || opt >= NELEM(setopt_vfptr, ep_setopt) || !setopt_vfptr[opt]) {
        errno = EINVAL;
        return -1;
    }
    rc = setopt_vfptr[opt] (ep, optval, optlen);
    return rc;
}

static int pub_ep_getopt(struct epbase *ep, int opt, void *optval, int *optlen)
{
    int rc;
    if (opt < 0 || opt >= NELEM(getopt_vfptr, ep_getopt) || !getopt_vfptr[opt]) {
        errno = EINVAL;
        return -1;
    }
    rc = getopt_vfptr[opt] (ep, optval, optlen);
    return rc;
}

static struct epbase_vfptr pub_epbase = {
    .sp_family = SP_PUBSUB,
    .sp_type = SP_PUB,
    .alloc = pub_ep_alloc,
    .destroy = pub_ep_destroy,
    .send = pub_ep_send,
    .add = pub_ep_add,
    .rm = pub_ep_rm,
    .join = pub_ep_join,
    .setopt = pub_ep_setopt,
    .getopt = pub_ep_getopt,
};

struct epbase_vfptr *pub_epbase_vfptr = &pub_epbase;

