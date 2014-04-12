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

/* URL example : 
 * net    group@net://182.33.49.10
 * ipc    group@ipc://xxxxxxxxxxxxxxxxxx
 * inproc group@inproc://xxxxxxxxxxxxxxxxxxxx
 */
static int url_parse_group(const char *url, char *buff, u32 size) {
    char *at = strchr(url, '@');;

    if (!at) {
	errno = EINVAL;
	return -1;
    }
    strncpy(buff, url, size <= at - url ? size : at - url);
    return 0;
}

static int url_parse_pf(const char *url) {
    char *at = strchr(url, '@');;
    char *pf = strstr(url, "://");;

    if (!at || !pf || at >= pf) {
	errno = EINVAL;
	return -1;
    }
    ++at;
    if (strncmp(at, "net", pf - at) == 0)
	return PF_NET;
    else if (strncmp(at, "ipc", pf - at) == 0)
	return PF_IPC;
    else if (strncmp(at, "inproc", pf - at) == 0)
	return PF_INPROC;
    errno = EINVAL;
    return -1;
}

static int url_parse_sockaddr(const char *url, char *buff, u32 size) {
    char *tok = "://";
    char *sock = strstr(url, tok);

    if (!sock) {
	errno = EINVAL;
	return -1;
    }
    sock += strlen(tok);
    strncpy(buff, sock, size);
    return 0;
}


int ep_connect(struct ep *ep, const char *url) {
    int pf = url_parse_pf(url);
    char sockaddr[URLNAME_MAX] = {};

    if (pf < 0 || url_parse_group(url, ep->h.group, URLNAME_MAX) < 0
	|| url_parse_sockaddr(url, sockaddr, URLNAME_MAX) < 0)
	return -1;
    return pxy_connect(ep->y, &ep->h, pf, sockaddr);
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
	return -1;
    }
    return 0;
}

char *ep_allocmsg(int size) {
    return channel_allocmsg(size);
}

void ep_freemsg(char *payload) {
    channel_freemsg(payload);
}
