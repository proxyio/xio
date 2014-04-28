#ifndef _HPIO_EPSTRUCT_
#define _HPIO_EPSTRUCT_

#include <base.h>
#include <sync/mutex.h>
#include <xio/ep.h>


#define XIO_MAX_EPS 10240

struct xep {

};

void xep_init(struct xep *ep);
void xep_exit(struct xep *ep);

struct xep_global {
    int exiting;
    mutex_t lock;

    /* The global table of existing xep. The descriptor representing
     * the xep is the index to this table. This pointer is also used to
     * find out whether context is initialised. If it is null, context
     * is uninitialised.
     */
    struct xep eps[XIO_MAX_EPS];

    /* Stack of unused xep descriptors.  */
    int unused[XIO_MAX_EPS];

    /* Number of actual socks. */
    size_t neps;
};


extern struct xep_global xepgb;






#endif
