#ifndef _HPIO_XGLOBAL_
#define _HPIO_XGLOBAL_

#include <sync/mutex.h>
#include <ds/list.h>
#include "xsock_struct.h"
#include "xcpu.h"


/* Max number of cpu core */
#define XSOCK_MAX_CPUS 32

struct xglobal {
    int exiting;
    mutex_t lock;

    /* The global table of existing xsock. The descriptor representing
       the xsock is the index to this table. This pointer is also used to
       find out whether context is initialised. If it is null, context is
       uninitialised. */
    struct xsock socks[XSOCK_MAX_SOCKS];

    /* Stack of unused xsock descriptors.  */
    int unused[XSOCK_MAX_SOCKS];

    /* Number of actual socks. */
    size_t nsocks;
    

    struct xcpu cpus[XSOCK_MAX_CPUS];

    /* Stack of unused xsock descriptors.  */
    int cpu_unused[XSOCK_MAX_CPUS];
    
    /* Number of actual runner poller.  */
    size_t ncpus;

    /* Backend cpu_cores and taskpool for cpu_worker.  */
    int cpu_cores;
    struct taskpool tpool;

    /* INPROC global listening address mapping */
    struct ssmap inproc_listeners;

    /* xsock_protocol head */
    struct list_head xsock_protocol_head;
};

#define xsock_protocol_walk_safe(pos, nx, head)			\
    list_for_each_entry_safe(pos, nx, head,			\
			     struct xsock_protocol, link)

struct xsock_protocol *l4proto_lookup(int pf, int type);

extern struct xglobal xgb;

static inline void xglobal_lock() {
    mutex_lock(&xgb.lock);
}

static inline void xglobal_unlock() {
    mutex_unlock(&xgb.lock);
}



#endif
