#include "base.h"
#include "channel_base.h"

extern struct channel_poll *pid_to_channel_poll(int pd);


int has_closed_channel(struct channel_poll *po) {
    int has = true;
    spin_lock(&po->lock);
    if (list_empty(&po->closing_head))
	has = false;
    spin_unlock(&po->lock);
    return has;
}

void push_closed_channel(struct channel *cn) {
    struct channel_poll *po = pid_to_channel_poll(cn->pollid);
    
    spin_lock(&po->lock);
    list_add_tail(&cn->closing_link, &po->closing_head);
    spin_unlock(&po->lock);
}

struct channel *pop_closed_channel(struct channel_poll *po) {
    struct channel *cn = NULL;

    spin_lock(&po->lock);
    if (!list_empty(&po->closing_head)) {
	cn = list_first(&po->closing_head, struct channel, closing_link);
	list_del_init(&cn->closing_link);
    }
    spin_unlock(&po->lock);
    return cn;
}

