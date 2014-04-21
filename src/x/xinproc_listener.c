#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "xbase.h"

/******************************************************************************
 *  xsock's proc field operation.
 ******************************************************************************/

struct xsock *find_listener(char *addr) {
    struct ssmap_node *node;
    struct xsock *sx = 0;

    xglobal_lock();
    if ((node = ssmap_find(&xgb.inproc_listeners, addr, TP_SOCKADDRLEN)))
	sx = cont_of(node, struct xsock, proc.rb_link);
    xglobal_unlock();
    return sx;
}

static int insert_listener(struct ssmap_node *node) {
    int rc = -1;

    errno = EADDRINUSE;
    xglobal_lock();
    if (!ssmap_find(&xgb.inproc_listeners, node->key, node->keylen)) {
	rc = 0;
	ssmap_insert(&xgb.inproc_listeners, node);
    }
    xglobal_unlock();
    return rc;
}


static void remove_listener(struct ssmap_node *node) {
    xglobal_lock();
    ssmap_delete(&xgb.inproc_listeners, node);
    xglobal_unlock();
}

/******************************************************************************
 *  xsock_inproc_protocol
 ******************************************************************************/

static int xinp_listener_init(int xd) {
    int rc;
    struct xsock *sx = xget(xd);
    struct ssmap_node *node = &sx->proc.rb_link;

    ZERO(sx->proc);
    INIT_LIST_HEAD(&sx->proc.at_queue);
    node->key = sx->addr;
    node->keylen = TP_SOCKADDRLEN;
    rc = insert_listener(node);
    return rc;
}

static void xinp_listener_destroy(int xd) {
    struct xsock *sx = xget(xd);

    /* Avoiding the new connectors */
    remove_listener(&sx->proc.rb_link);

    /* Destroy the xsock and free xsock id. */
    xsock_free(sx);
}

struct xsock_protocol xinp_listener_protocol = {
    .type = XLISTENER,
    .pf = PF_INPROC,
    .init = xinp_listener_init,
    .destroy = xinp_listener_destroy,
    .snd_notify = null,
    .rcv_notify = null,
};
