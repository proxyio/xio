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

#ifndef _HPIO_XPIPE_STRUCT_
#define _HPIO_XPIPE_STRUCT_

#include <base.h>
#include <ds/list.h>
#include <sync/mutex.h>
#include <xio/socket.h>
#include <xio/endpoint.h>
#include "ep_hdr.h"

#define XIO_MAX_ENDPOINTS 10240

struct endsock {
    int sockfd;
    uuid_t uuid;
    struct list_head link;
};

struct endpoint {
    int type;
    struct list_head bsocks;
    struct list_head csocks;
    struct list_head bad_socks;
};

#define xendpoint_walk_sock(ep, nep, head)				\
    list_for_each_entry_safe(ep, nep, head, struct endsock, link)

struct xep_global {
    int exiting;
    mutex_t lock;

    /* The global table of existing endpoint. The descriptor representing
     * the endpoint is the index to this table. This pointer is also used to
     * find out whether context is initialised. If it is null, context
     * is uninitialised.
     */
    struct endpoint endpoints[XIO_MAX_ENDPOINTS];

    /* Stack of unused endpoint descriptors.  */
    int unused[XIO_MAX_ENDPOINTS];

    /* Number of actual socks. */
    size_t nendpoints;
};


extern struct xep_global epgb;

int eid_alloc();
void eid_free(int eid);
struct endpoint *eid_get(int eid);
void accept_endsocks(int eid);

typedef struct endsock *(*target_algo) (struct endpoint *ep, char *ubuf);
extern const target_algo send_target_vfptr[];
extern const target_algo recv_target_vfptr[];

typedef int (*send_action) (struct endpoint *ep, struct endsock *sk, char *ubuf);
extern const send_action send_vfptr[];

typedef int (*recv_action) (struct endpoint *ep, struct endsock *sk, char **ubuf);
extern const recv_action recv_vfptr[];


#endif
