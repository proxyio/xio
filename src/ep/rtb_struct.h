#ifndef _HPIO_RTB_
#define _HPIO_RTB_

#include <uuid/uuid.h>
#include <base.h>
#include <x/xsock.h>
#include <ds/map.h>
#include <ds/list.h>
#include <runner/thread.h>
#include <sync/mutex.h>
#include "hdr.h"

#define URLNAME_MAX 128

struct ep_stat {
    u32 type;
    uuid_t ud;
    char group[URLNAME_MAX];
};

#define RECEIVER 1
#define DISPATCHER 2

struct fd;
struct xg;
struct rtb;

typedef void (*tb_event_handler) (struct fd *f, u32 events);

struct fd {
    void *self;
    int cd;
    struct xpoll_event event;
    tb_event_handler h;
    u32 fok:1;
    u64 mq_size;
    struct list_head mq;
    struct list_head link;

    /* For xg mapping */
    struct xg *g;
    struct ep_stat st;
    uuid_t uuid;
    ssmap_node_t rb_link;
};

#define fd_getself(f, type) ((type *)(f)->self)
#define fd_setself(f, __self) do {		\
	(f)->self = __self);			\
    } while (0)


#define list_for_each_fd(f, nx, head)				\
    list_for_each_entry_safe(f, nx, head, struct fd, link)

struct fd *fd_new();
void fd_free(struct fd *f);


struct xg {
    ssmap_t fdmap;
    int rsz;
    struct list_head rcver_head;
    int ssz;
    struct list_head snder_head;

    char group[URLNAME_MAX];
    ssmap_node_t rb_link;
    struct list_head link;
};


#define list_for_each_xg(g, ng, head)				\
    list_for_each_entry_safe(g, ng, head, struct xg, link)

struct xg *xg_new();
void xg_free(struct xg *g);
struct fd *xg_find(struct xg *g, uuid_t ud);
void xg_add(struct xg *g, struct fd *f);
void xg_rm(struct xg *g, struct fd *f);
struct fd *xg_rrbin_go(struct xg *g);
struct fd *xg_route_back(struct xg *g, uuid_t ud);


struct rtb {
    int size;
    struct list_head g_head;
    ssmap_t g_map;
};

struct rtb *rtb_new();
void rtb_free(struct rtb *tb);
void rtb_init(struct rtb *tb);
void rtb_destroy(struct rtb *tb);
int rtb_mapfd(struct rtb *tb, struct fd *f);
int rtb_unmapfd(struct rtb *tb, struct fd *f);
struct fd *rtb_rrbin_go(struct rtb *tb);
struct fd *rtb_route_back(struct rtb *tb, uuid_t ud);








#endif
