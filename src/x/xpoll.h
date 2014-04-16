#ifndef _HPIO_XPOLL_
#define _HPIO_XPOLL_

#include <sync/mutex.h>
#include <sync/spin.h>
#include <sync/condition.h>

struct xpoll_entry;
struct xpoll_notify {
    void (*event) (struct xpoll_notify *un, struct xpoll_entry *ent, u32 ev);
};

struct xpoll_entry {
    /* List item for linked other xpoll_entry of the same sock */
    struct list_head xlink;

    /* List item for linked other xpoll_entry of the same poll_table */
    struct list_head lru_link;

    spin_t lock;

    /* Reference hold by x/xpoll_t */
    int ref;

    /* Reference backtrace for debuging */
    int i_idx;
    int incr_tid[10];
    int d_idx;
    int desc_tid[10];
    
    /* struct xpoll_event contain the care events and what happened */
    struct xpoll_event event;
    struct xpoll_notify *notify;
};

#define xpoll_walk_ent(pos, nx, head)					\
    list_for_each_entry_safe(pos, nx, head, struct xpoll_entry, lru_link)

#define xsock_walk_ent(pos, nx, head)					\
    list_for_each_entry_safe(pos, nx, head, struct xpoll_entry, xlink)


struct xpoll_entry *xent_new();
int xent_get(struct xpoll_entry *ent);
int xent_put(struct xpoll_entry *ent);

struct xpoll_t {
    struct xpoll_notify notify;
    mutex_t lock;
    condition_t cond;

    /* Reference hold by xpoll_entry */
    int ref;
    int uwaiters;
    int size;
    struct list_head lru_head;
};

struct xpoll_t *xpoll_new();
int xpoll_get(struct xpoll_t *ut);
int xpoll_put(struct xpoll_t *ut);

struct xpoll_entry *xpoll_find(struct xpoll_t *po, int xd);
struct xpoll_entry *xpoll_popent(struct xpoll_t *po);
struct xpoll_entry *xpoll_getent(struct xpoll_t *po, int xd);
struct xpoll_entry *xpoll_putent(struct xpoll_t *po, int xd);

void attach_to_xsock(struct xpoll_entry *ent, int xd);
void __detach_from_xsock(struct xpoll_entry *ent);


#endif
