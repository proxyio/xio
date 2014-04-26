#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sync/waitgroup.h>
#include "runner/taskpool.h"
#include "xsock_struct.h"

/* Generic xpoll_t notify function. always called by xsock_protocol
 * when has any message come or can send any massage into network
 * or has a new connection wait for established.
 * here we only check the mq events and l4proto_spec saved the other
 * events gived by xsock_protocol
 */
void __xpoll_notify(struct xsock *sx, u32 l4proto_spec) {
    int events = 0;
    struct xpoll_entry *ent, *nx;

    events |= l4proto_spec;
    events |= !list_empty(&sx->rcv_head) ? XPOLLIN : 0;
    events |= can_send(sx) ? XPOLLOUT : 0;
    events |= !sx->fok ? XPOLLERR : 0;
    DEBUG_OFF("%d xsock events %d happen", sx->xd, events);
    xsock_walk_ent(ent, nx, &sx->xpoll_head) {
	ent->notify->event(ent->notify, ent, ent->event.care & events);
    }
}

void xpoll_notify(struct xsock *sx, u32 l4proto_spec) {
    mutex_lock(&sx->lock);
    __xpoll_notify(sx, l4proto_spec);
    mutex_unlock(&sx->lock);
}
