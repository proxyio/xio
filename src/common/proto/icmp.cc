#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <nspio/errno.h>
#include "os/time.h"
#include "proto/icmp.h"
#include "os/memalloc.h"

NSPIO_DECLARATION_START


struct nspiomsg *make_role_status_icmp(int dispatchers) {
    struct nspiomsg *msg = NULL;
    struct role_status_icmp status = {};
    int role_status_icmplen = sizeof(status);

    if (!(msg = (struct nspiomsg *)mem_zalloc(MSGHDRLEN + role_status_icmplen)))
	return NULL;
    msg->hdr.size = role_status_icmplen;
    msg->hdr.flags = PIOICMP_ROLESTATUS;
    msg->data = (char *)msg + MSGHDRLEN;
    status.dispatchers = dispatchers;
    memcpy(msg->data, &status, role_status_icmplen);
    return msg;
}


struct appmsg *make_deliver_status_icmp(struct appmsg *msg, int status) {
    int rtlen = msg->hdr.ttl * RTLEN;
    struct appmsg *icmp = NULL;
    struct deliver_status_icmp *deicmp = NULL;

    if (!(icmp = new_appmsg(NULL, sizeof(*deicmp) + rtlen)))
	return NULL;
    deicmp = (struct deliver_status_icmp *)icmp->s.data;
    deicmp->status = status;
    deicmp->hdr = msg->hdr;
    memcpy(deicmp->rts, msg->rt->rts, rtlen);
    
    icmp->hdr = msg->hdr;
    icmp->hdr.timestamp = rt_mstime();
    icmp->hdr.flags = APPICMP_DELIVERERROR;
    if (!(icmp->rt = appmsg_rtdup(msg->rt))) {
	free_appmsg(icmp);
	errno = ENOMEM;
	return NULL;
    }
    return icmp;
}

struct nspiomsg *make_deliver_status_icmp(struct nspiomsg *msg, int status) {
    struct nspiomsg *icmp = NULL;
    struct deliver_status_icmp *deicmp = NULL;
    int rtlen = msg->hdr.ttl * RTLEN;
    int msglen = sizeof(*icmp) + (sizeof(*deicmp) + rtlen) + rtlen;

    if (!(icmp = (struct nspiomsg *)mem_zalloc(msglen)))
	return NULL;
    icmp->hdr = msg->hdr;
    icmp->hdr.timestamp = rt_mstime();
    icmp->hdr.size = msglen - sizeof(*icmp);
    icmp->hdr.flags = APPICMP_DELIVERERROR;
    icmp->data = (char *)icmp + sizeof(*icmp);
    icmp->route = (struct spiort *)(icmp->data + icmp->hdr.size - RTLEN);
    memcpy(icmp->data + icmp->hdr.size - rtlen, msg->data + msg->hdr.size - rtlen, rtlen);
    
    deicmp = (struct deliver_status_icmp *)icmp->data;
    deicmp->status = status;
    deicmp->hdr = msg->hdr;
    memcpy(deicmp->rts, msg->data + msg->hdr.size - rtlen, rtlen);
    return icmp;
}


}
