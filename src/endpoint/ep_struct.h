#ifndef _HPIO_XPIPE_STRUCT_
#define _HPIO_XPIPE_STRUCT_

#include <base.h>
#include <ds/list.h>
#include <sync/mutex.h>
#include <xio/endpoint.h>
#include "ep_hdr.h"

#define XIO_MAX_ENDPOINTS 10240

struct endsock {
    int sockfd;
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

int efd_alloc();
void efd_free(int efd);
struct endpoint *efd_get(int efd);
void accept_incoming_endsocks(int efd);



#endif
