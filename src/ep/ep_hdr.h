#ifndef _HPIO_HDR_
#define _HPIO_HDR_

#include <errno.h>
#include <uuid/uuid.h>
#include <ds/list.h>
#include <os/timesz.h>
#include <x/xsock.h>
#include <hash/crc.h>

struct ep_rt {
    uuid_t uuid;
    u8 ip[4];
    u16 port;
    u16 begin[2];
    u16 cost[2];
    u16 stay[2];
};

struct ep_hdr {
    u8 version;
    u16 ttl:4;
    u16 end_ttl:4;
    u16 go:1;
    u32 size;
    u16 timeout;
    i64 sendstamp;
    u16 checksum;
    union {
	u64 __align[2];
	struct list_head link;
    } u;
    struct ep_rt rt[0];
};

static inline char *xtailmsg(char *msg) {
    return msg + xmsglen(msg);
}

static inline char *xmergemsg(char *x1, char *x2) {
    char *x3 = xallocmsg(xmsglen(x1) + xmsglen(x2));
    if (x3) {
	memcpy(x3, x1, xmsglen(x1));
	memcpy(x3 + xmsglen(x1), x2, xmsglen(x2));
    }
    return x3;
}

struct ep_hdr *ep_alloc_req(char *req);
void ep_freehdr(struct ep_hdr *h);


#define list_for_each_eh(h, nh, head)					\
    list_for_each_entry_safe(h, nh, head, struct ep_hdr, u.link)

static inline u32 ep_hds(struct ep_hdr *h) {
    u32 ttl = h->go ? h->ttl : h->end_ttl;
    return sizeof(*h) + ttl * sizeof(struct ep_rt);
}

static inline int ep_hdr_timeout(struct ep_hdr *h) {
    return h->timeout && (h->sendstamp + h->timeout < rt_mstime());
}

static inline int ep_hdr_validate(struct ep_hdr *h) {
    int ok;
    struct ep_hdr copyheader = *h;

    copyheader.checksum = 0;
    if (!(ok = (crc16((char *)&copyheader, sizeof(*h)) == h->checksum)))
	errno = EPROTO;
    return ok;
}

static inline void ep_hdr_gensum(struct ep_hdr *h) {
    struct ep_hdr copyh = *h;

    copyh.checksum = 0;
    h->checksum = crc16((char *)&copyh, sizeof(copyh));
}

static inline struct ep_rt *rt_cur(struct ep_hdr *h) {
    return &h->rt[h->ttl - 1];
}

static inline struct ep_rt *rt_prev(struct ep_hdr *h) {
    return &h->rt[h->ttl - 2];
}

static inline void rt_go_cost(struct ep_hdr *h) {
    struct ep_rt *cr = rt_cur(h);
    cr->cost[0] = (u16)(rt_mstime() - h->sendstamp - cr->begin[0]);
}

static inline void rt_back_cost(struct ep_hdr *h) {
    struct ep_rt *cr = rt_cur(h);
    cr->cost[1] = (u16)(rt_mstime() - h->sendstamp - cr->begin[1]);
}

struct ep_hdr *rt_append_and_go(struct ep_hdr *h, struct ep_rt *r);
void rt_shrink_and_back(struct ep_hdr *h);



static inline int ep_recv(int xd, struct ep_hdr **h) {
    if (xrecv(xd, (char **)h) < 0)
	return -1;
    if (ep_hdr_timeout(*h) < 0) {
	errno = EAGAIN;
	xfreemsg(*(char **)h);
	DEBUG_OFF("message is timeout");
	return -1;
    }
    if (ep_hdr_validate(*h) < 0) {
	errno = EPIPE;
	xfreemsg(*(char **)h);
	DEBUG_OFF("invalid message's checksum");
	return -1;
    }
    return 0;
}


static inline int ep_send(int xd, struct ep_hdr *h) {
    return xsend(xd, (char *)h);
}



















#endif
