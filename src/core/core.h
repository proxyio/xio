#ifndef _HPIO_CORE_
#define _HPIO_CORE_

#include <inttypes.h>
#include <uuid/uuid.h>
#include "sync/atomic.h"
#include "ds/map.h"
#include "os/epoll.h"
#include "os/malloc.h"
#include "sync/spin.h"
#include "stats/modstat.h"
#include "proto/parser.h"
#include "runner/taskpool.h"

typedef struct accepter {
    spin_t lock;
    taskpool_t tp;
    epoll_t el;
    epollevent_t et;
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

enum {
    ST_REGISTED = 1,
    ST_OK = 2,
    ST_REGISTER = 4,
};

struct rio {
    spin_t lock;
    int status;
    atomic_t ref;
    uuid_t uuid;
    uint32_t type;
    modstat_t st;
    epollevent_t et;
    epoll_t *el;
    proxy_t *py;
    uint32_t size;
    struct list_head mq;
    struct io conn_ops;
    struct pio_parser pp;
    ssmap_node_t sibling;
    struct list_head py_link;
};

#define list_for_each_role_safe(pos, n, head)				\
    list_for_each_entry_safe(pos, n, head, struct rio, py_link)


#endif
