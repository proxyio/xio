#ifndef _HPIO_CORE_
#define _HPIO_CORE_

#include <inttypes.h>
#include <uuid/uuid.h>
#include "sync/atomic.h"
#include "ds/map.h"
#include "os/epoll.h"
#include "os/slab.h"
#include "sync/spin.h"
#include "stats/modstat.h"
#include "runner/taskpool.h"
#include "hdr.h"
#include "proto_parser.h"

#define lock(o) do {				\
	spin_lock(&o->lock);			\
    } while (0)

#define unlock(o) do {				\
	spin_unlock(&o->lock);			\
    } while (0)


struct cf {
    int tp_workers;
    int el_io_size;
    int el_wait_timeout;
    char *monitor_center;
};

extern struct cf default_cf;

typedef struct accepter {
    spin_t lock;
    taskpool_t tp;
    epoll_t el;
    struct cf cf;
    struct list_head sub_el_head;
    struct list_head et_head;
    struct list_head py_head;
} acp_t;

typedef struct proxy {
    spin_t lock;
    char name[PROXYNAME_MAX];
    int rsize;
    ssmap_t roles;
    struct list_head acp_link;
    struct list_head rcver_head;
    struct list_head snder_head;
} proxy_t;

#define list_for_each_py_safe(pos, n, head)			\
    list_for_each_entry_safe(pos, n, head, proxy_t, acp_link)

struct role {
    proto_parser_t pp;
    spin_t lock;
    uint32_t registed:1;
    uint32_t status_ok:1;
    uint32_t proxyto:1;
    atomic_t ref;
    modstat_t st;
    epollevent_t et;
    epoll_t *el;
    mem_cache_t slabs;
    uint32_t mqsize;
    struct list_head mq;
    acp_t *acp;
    proxy_t *py;
    ssmap_node_t sibling;
    struct list_head py_link;
};

#define list_for_each_role_safe(pos, n, head)				\
    list_for_each_entry_safe(pos, n, head, struct role, py_link)


#endif
