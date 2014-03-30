#include "base.h"
#include "transport/tcp/tcp.h"
#include "transport/ipc/ipc.h"
#include "transport/transport.h"


static struct list_head _transport_head = {};
struct list_head *transport_head = &_transport_head;

/* List all available transport here */
extern struct transport *tcp_transport;
extern struct transport *ipc_transport;

#define list_for_each_transport(tp)					\
    list_for_each_entry(tp, transport_head, struct transport, item)

static inline void add_transport(struct transport *tp) {
    tp->global_init();
    list_add(&tp->item, transport_head);
}

void global_transport_init() {
    INIT_LIST_HEAD(transport_head);
    add_transport(tcp_transport);
    add_transport(ipc_transport);
}

void global_transport_exit() {
}


struct transport *transport_lookup(int pf) {
    struct transport *tp = NULL;

    list_for_each_transport(tp)
	if (tp->proto == pf)
	    return tp;
    return NULL;
}
