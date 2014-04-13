#include <errno.h>
#include <uuid/uuid.h>
#include <os/alloc.h>
#include <channel/channel.h>
#include "hdr.h"
#include "ds/list.h"
#include "hash/crc.h"


void gsm_init(struct gsm *s, char *payload) {
    INIT_LIST_HEAD(&s->link);
    if (payload) {
	s->payload = payload;
	s->h = (struct rdh *)payload;
	s->r = (struct tr *)(payload + sizeof(*s->h) + s->h->size);
    }
}

struct gsm *gsm_new(char *payload) {
    struct gsm *s = (struct gsm *)mem_zalloc(sizeof(*s));
    if (s)
	gsm_init(s, payload);
    return s;
}

void gsm_free(struct gsm *s) {
    if (s->payload)
	channel_freemsg(s->payload);
    mem_free(s, sizeof(struct gsm));
}

int gsm_validate(struct gsm *s) {
    struct rdh *h = s->h;
    struct rdh copyheader = *h;
    int ok;

    copyheader.checksum = 0;
    if (!(ok = (crc16((char *)&copyheader, sizeof(*h)) == h->checksum)))
	errno = EPROTO;
    return ok;
}

void gsm_gensum(struct gsm *s) {
    struct rdh *h = s->h;
    struct rdh copyheader = *h;

    copyheader.checksum = 0;
    h->checksum = crc16((char *)&copyheader, sizeof(*h));
}

int tr_append_and_go(struct gsm *s, struct tr *r, i64 now) {
    struct tr *cr;
    struct rdh *h = s->h;
    char *new_payload, *payload_end;
    
    cr = tr_cur(s);
    cr->stay[0] = (u16)(now - h->sendstamp - cr->begin[0]);
    new_payload = channel_allocmsg(channel_msglen(s->payload) + sizeof(*r));
    if (!new_payload)
	return -1;
    memcpy(new_payload, s->payload, channel_msglen(s->payload));
    channel_freemsg(s->payload);
    s->payload = new_payload;

    /* The new header and route */
    h = s->h = (struct rdh *)s->payload;
    payload_end = s->payload + channel_msglen(s->payload);
    h->ttl++;
    s->r = (struct tr *)(payload_end - tr_size(s->h));

    /* Copy the new route info */
    cr = tr_cur(s);
    *cr = *r;
    cr->begin[0] = (u16)(now - h->sendstamp);
    gsm_gensum(s);
    return 0;
}

void tr_shrink_and_back(struct gsm *s, i64 now) {
    struct tr *r = tr_cur(s);
    struct rdh *h = s->h;
    r->stay[1] = (now - h->sendstamp - r->begin[1] - r->cost[1]);
    h->ttl--;
    gsm_gensum(s);
    r = tr_cur(s);
    r->begin[1] = (u16)(now - h->sendstamp);
}
