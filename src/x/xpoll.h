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
    /* List item for linked other xpoll_entry of the same channel */
    struct list_head xlink;

    /* List item for linked other xpoll_entry of the same poll_table */
    struct list_head lru_link;

    spin_t lock;

    /* Reference hold by channel/xpoll_t */
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

#define list_for_each_xpoll_ent(pos, nx, head)				\
    list_for_each_entry_safe(pos, nx, head, struct xpoll_entry, lru_link)

#define list_for_each_xent(pos, nx, head)				\
    list_for_each_entry_safe(pos, nx, head, struct xpoll_entry, xlink)

struct xpoll_entry *entry_new();
int entry_get(struct xpoll_entry *ent);
int entry_put(struct xpoll_entry *ent);

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

struct xpoll_t *po_new();
int po_get(struct xpoll_t *ut);
int po_put(struct xpoll_t *ut);

struct xpoll_entry *po_find(struct xpoll_t *po, int cd);
struct xpoll_entry *po_popent(struct xpoll_t *po);
struct xpoll_entry *po_getent(struct xpoll_t *po, int cd);
struct xpoll_entry *po_putent(struct xpoll_t *po, int cd);

void attach_to_channel(struct xpoll_entry *ent, int cd);
void __detach_from_channel(struct xpoll_entry *ent);


#endif
