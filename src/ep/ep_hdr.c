#include <errno.h>
#include <uuid/uuid.h>
#include <os/alloc.h>
#include <x/xsock.h>
#include "ds/list.h"
#include "hash/crc.h"
#include "ep_hdr.h"

int ep_hdr_validate(struct ep_hdr *h) {
    struct ep_hdr copyheader = *h;
    int ok;

    copyheader.checksum = 0;
    if (!(ok = (crc16((char *)&copyheader, sizeof(*h)) == h->checksum)))
	errno = EPROTO;
    return ok;
}

void ep_hdr_gensum(struct ep_hdr *h) {
    struct ep_hdr copyheader = *h;

    copyheader.checksum = 0;
    h->checksum = crc16((char *)&copyheader, sizeof(*h));
}

struct ep_hdr *rt_append_and_go(struct ep_hdr *h, struct ep_rt *r) {
    struct ep_rt *cr;
    u64 now = rt_mstime();
    struct ep_hdr *nh;
    
    cr = rt_cur(h);
    cr->stay[0] = (u16)(now - h->sendstamp - cr->begin[0]);
    if (!(nh = (struct ep_hdr *)xallocmsg(xmsglen((char *)h) + sizeof(*r))))
	return 0;
    memcpy(nh, h, hdr_size(h));
    memcpy((char *)nh + hdr_size(h) + sizeof(*r),
	   (char *)h + hdr_size(h), h->size);
    xfreemsg((char *)h);
    nh->ttl++;
    cr = rt_cur(nh);
    *cr = *r;
    cr->begin[0] = (u16)(now - nh->sendstamp);
    ep_hdr_gensum(nh);
    return nh;
}

void rt_shrink_and_back(struct ep_hdr *h) {
    struct ep_rt *cr = rt_cur(h);
    u64 now = rt_mstime();

    cr->stay[1] = (now - h->sendstamp - cr->begin[1] - cr->cost[1]);
    h->ttl--;
    ep_hdr_gensum(h);
    cr = rt_cur(h);
    cr->begin[1] = (u16)(now - h->sendstamp);
}
