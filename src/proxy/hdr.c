#include <errno.h>
#include <uuid/uuid.h>
#include <os/alloc.h>
#include <channel/channel.h>
#include "hdr.h"
#include "ds/list.h"
#include "hash/crc.h"


struct gsm *gsm_new(char *payload) {
    struct gsm *s = (struct gsm *)mem_zalloc(sizeof(*s));
    if (s) {
	INIT_LIST_HEAD(&s->link);
	s->payload = payload;
	s->h = (struct rdh *)payload;
	s->r = (struct tr *)(payload + sizeof(*s->h) + s->h->size);
    }
    return s;
}

void gsm_free(struct gsm *s) {
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

int tr_append_and_go(struct gsm *s, struct tr *r, int64_t now) {
    struct tr *cr;
    struct rdh *h;
    char *new_payload, *payload_end;
    
    new_payload = channel_allocmsg(channel_msglen(s->payload) + sizeof(*r));
    if (!new_payload)
	return -1;
    memcpy(new_payload, s->payload, channel_msglen(s->payload));
    channel_freemsg(s->payload);
    s->payload = new_payload;
    s->h = h = (struct rdh *)s->payload;
    payload_end = s->payload + channel_msglen(s->payload);
    s->r = (struct tr *)(payload_end - tr_size(s->h));
    cr = tr_cur(s);
    cr->stay[0] = (uint16_t)(now - h->sendstamp - cr->begin[0]);
    h = s->h;
    h->ttl++;
    cr = tr_cur(s);

    /* Copy the new route info */
    *cr = *r;
    cr->begin[0] = (uint16_t)(now - h->sendstamp);
    gsm_gensum(s);
    return 0;
}

void tr_shrink_and_back(struct gsm *s, int64_t now) {
    struct tr *r = tr_cur(s);
    struct rdh *h = s->h;
    r->stay[1] = (now - h->sendstamp - r->begin[1] - r->cost[1]);
    h->ttl--;
    gsm_gensum(s);
    r = tr_cur(s);
    r->begin[1] = (uint16_t)(now - h->sendstamp);
}
