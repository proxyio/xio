#ifndef _HPIO_CORE_
#define _HPIO_CORE_

#include <inttypes.h>
#include <uuid/uuid.h>
#include "ds/map.h"
#include "os/epoll.h"
#include "os/malloc.h"
#include "sync/spin.h"
#include "stats/modstat.h"
#include "proto/parser.h"


typedef struct accepter {
    spin_t lock;
    epoll_t el;
    epollevent_t et;
    struct list_head grp_head;
} accepter_t;

typedef struct grp {
    char grpname[GRPNAME_MAX];
    spin_t lock;
    int rsize;
    ssmap_t roles;
    ssmap_t tw_roles;
    struct list_head rcver_head;
    struct list_head snder_head;
    struct list_head ctx_link;
} grp_t;

enum {
    ST_REGISTED = 1,
    ST_OK = 2,
    ST_REGISTER = 4,
};

struct role {
    int status;
    uuid_t uuid;
    uint32_t type;
    spin_t lock;
    modstat_t st;
    epollevent_t et;
    epoll_t *el;
    grp_t *grp;
    uint32_t size;
    struct list_head mq_link;
    ssmap_node_t sibling;
    struct list_head grp_link;
    struct io conn_ops;
    struct pio_parser pp;
};

#define list_for_each_role_safe(pos, n, head)				\
    list_for_each_entry_safe(pos, n, head, struct role, grp_link)


#endif
