#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <runner/taskpool.h>
#include "xgb.h"

/******************************************************************************
 *  xsock's proc field operation.
 ******************************************************************************/

struct xsock *find_listener(const char *addr) {
    struct ssmap_node *node;
    struct xsock *sx = 0;
    u32 size = strlen(addr);

    if (size > TP_SOCKADDRLEN)
	size = TP_SOCKADDRLEN;
    xglobal_lock();
    if ((node = ssmap_find(&xgb.inproc_listeners, addr, size)))
	sx = cont_of(node, struct xsock, proc.rb_link);
    xglobal_unlock();
    return sx;
}

static int insert_listener(struct ssmap_node *node) {
    int rc = -1;

    xglobal_lock();
    if (!ssmap_find(&xgb.inproc_listeners, node->key, node->keylen)) {
	rc = 0;
	DEBUG_OFF("insert listener %s", node->key);
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

static int xinp_listener_bind(int xd, const char *sock) {
    int rc;
    struct ssmap_node *node = 0;
    struct xsock *sx = xget(xd);

    ZERO(sx->proc);
    strncpy(sx->addr, sock, TP_SOCKADDRLEN);

    node = &sx->proc.rb_link;
    node->key = sx->addr;
    node->keylen = strlen(sx->addr);
    if ((rc = insert_listener(node)) < 0) {
	errno = EADDRINUSE;
	return -1;
    }
    return 0;
}

static void xinp_listener_close(int xd) {
    struct xsock *sx = xget(xd);

    /* Avoiding the new connectors */
    remove_listener(&sx->proc.rb_link);

    /* Close the xsock and free xsock id. */
    xsock_free(sx);
}

struct xsock_protocol xinp_listener_protocol = {
    .type = XLISTENER,
    .pf = XPF_INPROC,
    .bind = xinp_listener_bind,
    .close = xinp_listener_close,
    .snd_notify = null,
    .rcv_notify = null,
};
