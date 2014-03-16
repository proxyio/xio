#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "net/socket.h"
#include "proto_parser.h"

const char *pp_modstat_item[PP_MODSTAT_KEYRANGE] = {
    "RECV",
    "SEND",
    "ERROR",
    "RTT",
    "RECONNECT",
};

static inline int64_t sock_read(struct io *sock, char *buff, int64_t sz) {
    proto_parser_t *pp = container_of(sock, proto_parser_t, sock_ops);
    sz = sk_read(pp->sockfd, buff, sz);
    return sz;
}

static inline int64_t sock_write(struct io *sock, char *buff, int64_t sz) {
    proto_parser_t *pp = container_of(sock, proto_parser_t, sock_ops);
    sz = sk_write(pp->sockfd, buff, sz);
    return sz;
}


void proto_parser_init(proto_parser_t *pp) {
    ZERO(*pp);
    INIT_MODSTAT(pp->stat);
    bio_init(&pp->in);
    bio_init(&pp->out);
    pp->sock_ops.read = sock_read;
    pp->sock_ops.write = sock_write;
}

void proto_parser_destroy(proto_parser_t *pp) {
    bio_destroy(&pp->in);
    bio_destroy(&pp->out);
}


int proto_parser_ps_rgs(proto_parser_t *pp) {
    struct bio *b = &pp->in;
    modstat_t *stat = proto_parser_stat(pp);

    while ((b->bsize < sizeof(pp->rgh)))
	if (bio_prefetch(b, &pp->sock_ops) < 0)
	    return -1;
    modstat_incrkey(stat, PP_RECONNECT);
    bio_read(b, (char *)&pp->rgh, sizeof(pp->rgh));
    return 0;
}

int proto_parser_at_rgs(proto_parser_t *pp) {
    struct bio *b = &pp->out;
    modstat_t *stat = proto_parser_stat(pp);

    bio_write(b, (char *)&pp->rgh, sizeof(pp->rgh));
    bio_flush(b, &pp->sock_ops);
    modstat_incrkey(stat, PP_RECONNECT);
    return 0;
}


int proto_parser_bread(proto_parser_t *pp,
		       struct pio_hdr *h, char **data, char **rt) {
    struct bio *b = &pp->in;
    modstat_t *stat = proto_parser_stat(pp);

    if (!proto_parser_one_ready(pp)) {
	errno = EAGAIN;
	return -1;
    }
    modstat_incrkey(stat, PP_RECV);
    bio_copy(b, (char *)h, sizeof(*h));
    if (!(*data = (char *)mem_zalloc(h->size)))
	return -1;
    if (!(*rt = (char *)mem_zalloc(rt_size(h)))) {
	mem_free(*data, h->size);
	*data = NULL;
	return -1;
    }
    bio_read(b, (char *)h, sizeof(*h));
    bio_read(b, *data, h->size);
    bio_read(b, *rt, rt_size(h));
    return 0;
}


int proto_parser_bwrite(proto_parser_t *pp,
			const struct pio_hdr *h, const char *data, const char *rt) {
    modstat_t *stat = proto_parser_stat(pp);

    modstat_incrkey(stat, PP_SEND);
    bio_write(&pp->out, (char *)h, sizeof(*h));
    bio_write(&pp->out, data, h->size);
    bio_write(&pp->out, rt, rt_size(h));
    return 0;
}

