#include <stdio.h>
#include "ep.h"
#include "ep_hdr.h"


struct ep_hdr *ep_alloc_req(char *req) {
    u32 hds = sizeof(struct ep_hdr) + sizeof(struct ep_rt);
    struct ep_hdr *eh = (struct ep_hdr *)xallocmsg(xmsglen(req) + hds);
    if (eh) {
	eh->version = 0;
	eh->ttl = 1;
	eh->end_ttl = 0;
	eh->go = true;
	eh->size = xmsglen(req);
	eh->timeout = 0;
	eh->checksum = 0;
	eh->sendstamp = rt_mstime();

	memcpy((char *)eh + ep_hds(eh), req, xmsglen(req));
	xfreemsg(req);
	ep_hdr_gensum(eh);
    }
    return eh;
}

void ep_freehdr(struct ep_hdr *h) {
    xfreemsg((char *)h);
}


