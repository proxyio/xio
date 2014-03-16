#ifndef _HPIO_BC_
#define _HPIO_BC_

#include "os/epoll.h"
#include "ds/list.h"
#include "core/proto_parser.h"

typedef struct pingpong_ctx {
    struct bc_opt *cf;
    epoll_t el;
    struct list_head io_head;
} pingpong_ctx_t;


static inline void
bc_threshold_warn(modstat_t *self, int sl, int key, int64_t ts, int64_t val) {
    printf("[%s %s %ld]\n", stat_level_token[sl], pp_modstat_item[key], val);
}


#endif
