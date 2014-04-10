#include <errno.h>
#include <uuid/uuid.h>
#include "hdr.h"
#include "ds/list.h"
#include "hash/crc.h"

int gsm_validate(struct rdh *h) {
    struct rdh copyheader = *h;
    int ok;

    copyheader.checksum = 0;
    if (!(ok = (crc16((char *)&copyheader, sizeof(*h)) == h->checksum)))
	errno = EPROTO;
    return ok;
}

void gsm_gensum(struct rdh *h) {
    struct rdh copyheader = *hdr;
    copyheader.checksum = 0;
    hdr->checksum = crc16((char *)&copyheader, sizeof(*h));
}

int tr_append_and_go(struct gsm *s, struct tr *r, int64_t now) {
    struct tr *r;
    struct rdh *h;
    char *new_payload;

    if (!(new_payload = channel_allocmsg(gsm_size(s) + sizeof(*r))))
	return -1;
    memcpy(new_payload, s->payload, gsm_size(s));
    channel_freemsg(s->payload);
    s->payload = new_payloadk;
    s->h = (struct rdh *)s->payload;
    s->r = (struct tr *)(s->payload + gsm_size(s) - tr_size(s->h));
    r = tr_cur(s);
    r->stay[0] = (uint16_t)(now - h->sendstamp - r->begin[0]);
    h = s->h;
    h->ttl++;
    r = tr_cur(s);
    r->begin[0] = (uint16_t)(now - h->sendstamp);
    gsm_gensum(h);
    return 0;
}

void tr_shrink_and_back(struct gsm *s, int64_t now) {
    struct tr *r = tr_cur(s);
    struct rdh *h = s->h;
    r->stay[1] = (now - h->sendstamp - r->begin[1] - r->cost[1]);
    h->ttl--;
    gsm_gensum(h);
    r = tr_cur(s);
    r->begin[1] = (uint16_t)(now - h->sendstamp);
}
