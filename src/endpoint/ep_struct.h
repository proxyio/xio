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
#include <sync/spin.h>
#include <runner/thread.h>
#include <xio/socket.h>
#include <xio/poll.h>
#include <xio/endpoint.h>
#include "ep_hdr.h"

#define XIO_MAX_ENDPOINTS 10240

struct endpoint;

struct endsock {
    struct endpoint *owner;
    struct xpoll_event ent;
    int sockfd;
    uuid_t uuid;
    struct list_head link;
};

struct endpoint {
    struct xproxy *owner;
    int type;
    struct list_head bsocks;
    struct list_head csocks;
    struct list_head bad_socks;
};

#define xendpoint_walk_sock(ep, nep, head)				\
    list_for_each_entry_safe(ep, nep, head, struct endsock, link)

#ifdef __cplusplus
extern "C" {
#endif

extern void eid_strace(int eid);

#ifdef __cplusplus
}
#endif

struct endsock *__xep_add(int eid, int sockfd);
struct endsock *endpoint_accept(int eid, struct endsock *sk);

struct xproxy {
    int exiting;
    spin_t lock;
    struct xpoll_t *po;
    struct endpoint *frontend;
    struct endpoint *backend;
    thread_t py_worker;
};

struct xproxy *xproxy_open(int frontend_eid, int backend_eid);
void xproxy_close(struct xproxy *py);

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
int ep2eid(struct endpoint *ep);
void accept_endsocks(int eid);

typedef struct endsock *(*target_algo) (struct endpoint *ep, char *ubuf);
extern const target_algo send_target_vfptr[];
extern const target_algo recv_target_vfptr[];

typedef int (*send_action) (struct endsock *sk, char *ubuf);
extern const send_action send_vfptr[];

typedef int (*recv_action) (struct endsock *sk, char **ubuf);
extern const recv_action recv_vfptr[];


#endif
