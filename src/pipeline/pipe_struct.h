#ifndef _HPIO_XPIPE_STRUCT_
#define _HPIO_XPIPE_STRUCT_

#include <base.h>
#include <ds/list.h>
#include <sync/mutex.h>
#include <xio/pipeline.h>


#define XIO_MAX_PIPES 10240

struct endpoint {
    int sockfd;
    struct list_head link;
};

struct pipe_struct {
    struct list_head endpoints;
};

#define xpipe_walk_endpoint(ep, nep, head)				\
    list_for_each_entry_safe(ep, nep, head, struct endpoint, link)

void xpipe_init(struct pipe_struct *ps);
void xpipe_exit(struct pipe_struct *ps);

struct xpipe_global {
    int exiting;
    mutex_t lock;

    /* The global table of existing xpipe. The descriptor representing
     * the xpipe is the index to this table. This pointer is also used to
     * find out whether context is initialised. If it is null, context
     * is uninitialised.
     */
    struct pipe_struct pipes[XIO_MAX_PIPES];

    /* Stack of unused xpipe descriptors.  */
    int unused[XIO_MAX_PIPES];

    /* Number of actual socks. */
    size_t npipes;
};


extern struct xpipe_global xpgb;

int pfd_alloc();
void pfd_free(int pfd);
struct pipe_struct *pfd_get(int pfd);




#endif
