#ifndef _HPIO_CHANNEL_POLL_
#define _HPIO_CHANNEL_POLL_


/* EXPORT user poll implementation, different from channel_poll */

#define UPOLLIN   1
#define UPOLLOUT  2
#define UPOLLERR  4

struct upoll_event {
    int cd;

    /* What events i care about ... */
    uint32_t care;

    /* What events happened now */
    uint32_t happened;
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

    /* struct upoll_event contain the register poll events and what happened */
    struct upoll_event event;
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

#define UPOLL_ADD 1
#define UPOLL_DEL 2
#define UPOLL_MOD 3

/* All channel must be remove from upoll before closed
 * Can't modify an unexist channel.
 */
int upoll_ctl(struct upoll_table *ut, int op, struct upoll_event *ue);

int upoll_wait(struct upoll_table *ut, struct upoll_event *events, int n, int timeout);







#endif
