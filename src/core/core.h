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
#include "sdk/c/proto.h"
#include "sdk/c/proxyio.h"


typedef struct accepter {
    spin_t lock;
    taskpool_t tp;
    epoll_t el;
    struct list_head et_head;
    struct list_head py_head;
} acp_t;

typedef struct proxy {
    spin_t lock;
    char proxyname[PROXYNAME_MAX];
    int rsize;
    ssmap_t roles;
    ssmap_t tw_roles;
    struct list_head acp_link;
    struct list_head rcver_head;
    struct list_head snder_head;
} proxy_t;


struct rio {
    proxyio_t io;
    spin_t lock;
    uint32_t registed:1;
    uint32_t status_ok:1;
    uint32_t is_register:1;
    uint32_t flags:29;
    atomic_t ref;
    modstat_t st;
    epollevent_t et;
    epoll_t *el;
    proxy_t *py;
    uint32_t mqsize;
    struct list_head mq;
    ssmap_node_t sibling;
    struct list_head py_link;
    mem_cache_t slabs;
};

#define list_for_each_role_safe(pos, n, head)			\
    list_for_each_entry_safe(pos, n, head, struct rio, py_link)


#endif
