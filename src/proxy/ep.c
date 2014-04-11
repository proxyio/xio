#include <os/alloc.h>
#include <channel/channel.h>
#include "pxy.h"
#include "ep.h"

#define DEFAULT_GROUP "default"

struct ep {
    struct hgr h;
    struct pxy *y;
};

struct ep *ep_new(int ty) {
    struct ep *ep = (struct ep *)mem_zalloc(sizeof(*ep));
    if (ep) {
	if (!(ep->y = pxy_new())) {
	    mem_free(ep, sizeof(*ep));
	    return 0;
	}
	uuid_generate(ep->h.id);
	ep->h.type = ty;
	strcpy(ep->h.group, DEFAULT_GROUP);
    }
    return ep;
}

void ep_close(struct ep *ep) {
    pxy_free(ep->y);
    mem_free(ep, sizeof(*ep));
}

/* url example : group@net:://182.33.49.10 */

int ep_connect(struct ep *ep, const char *url) {
    int rc, pf;
    char *pos, *url2, *url3;

    url2 = url3 = strdup(url);

    /* Parse group name */
    if ((pos = strchr(url2, '@'))) {
	pos[0] = '\0';
	strcpy(ep->h.group, url2);
	url2 = ++pos;
    }
    if (!(pos = strstr(url2, ":://"))) {
	errno = EINVAL;
	free(url3);
	return -1;
    }
    pos[0] = '\0';
    if (strcmp(url2, "net") == 0)
	pf = PF_NET;
    else if (strcmp(url2, "inp") == 0)
	pf = PF_INPROC;
    else if (strcmp(url2, "ipc") == 0)
	pf = PF_IPC;
    else {
	errno = EINVAL;
	free(url3);
	return -1;
    }
    url2 = pos + 4;
    rc = pxy_connect(ep->y, &ep->h, pf, url2);
    free(url3);
    return rc;
}

char *ep_recv(struct ep *ep) {
    if (ep->h.type & (PRODUCER|COMSUMER)) {
	errno = EINVAL;
	return 0;
    }
    return 0;
}

int ep_send(struct ep *ep, char *payload) {
    if (ep->h.type & (PRODUCER|COMSUMER)) {
	errno = EINVAL;
	return 0;
    }
    return 0;
}

char *ep_allocmsg(int size) {
    return channel_allocmsg(size);
}

void ep_freemsg(char *payload) {
    channel_freemsg(payload);
}
