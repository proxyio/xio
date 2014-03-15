#ifndef _HPIO_BC_
#define _HPIO_BC_

#include "os/epoll.h"
#include "ds/list.h"

typedef struct pingpong_ctx {
    struct bc_opt *cf;
    epoll_t el;
    struct list_head io_head;
} pingpong_ctx_t;

#endif
