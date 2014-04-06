#ifndef _HPIO_CHANNEL_POLL_
#define _HPIO_CHANNEL_POLL_


/* EXPORT user poll implementation, different from channel_poll */

#define UPOLLIN   1
#define UPOLLOUT  2
#define UPOLLERR  4

struct pollcn {
    int cd;
    uint32_t events;
    uint32_t events_happened;
};

struct upoll_entry;
struct upoll_notify {
    void (*event) (struct upoll_notify *un, struct upoll_entry *ent);
};

struct upoll_entry {
    /* List item for linked other upoll_entry of the same channel */
    struct list_head channel_link;

    /* List item for linked other upoll_entry of the same poll_table */
    struct list_head polltable_link;

    /* List item for linked other upoll_entry that has any event happened */
    struct list_head happened_link;

    /* struct pollcn contain the register poll events and what happened */
    struct pollcn cn;
    struct upoll_notify *notify;
};


struct upoll_table {
    struct upoll_notify notify;
    int uwaiters;
    mutex_t lock;
    condition_t cond;
    /* List header used to save all happened upoll_entry */
    int cur_happened_events;
    struct list_head happened_entries;

    /* List header used to save all register upoll_entry */
    int ent_size;
    struct list_head poll_entries;
};

#define list_for_each_upollentry(pos, nx, head)				\
    list_for_each_entry_safe(pos, nx, head, struct upoll_entry, polltable_link)


struct upoll_table *upoll_create();
void upoll_close(struct upoll_table *ut);
int upoll_ctl(struct upoll_table *ut, struct pollcn *cn);
int upoll_wait(struct upoll_table *ut, struct pollcn *cns, int ncn, int timeout);







#endif
