#include <stdio.h>
#include <os/alloc.h>
#include "ep_struct.h"

void xep_close(int eid) {
    struct list_head closed_head;
    struct endpoint *ep = eid_get(eid);
    struct endsock *es, *next_es;

    eid_free(eid);
    INIT_LIST_HEAD(&closed_head);
    list_splice(&ep->bsocks, &closed_head);
    list_splice(&ep->csocks, &closed_head);
    list_splice(&ep->bad_socks, &closed_head);
    xendpoint_walk_sock(es, next_es, &closed_head) {
	xclose(es->sockfd);
	list_del_init(&es->link);
	mem_free(es, sizeof(*es));
    }
}
