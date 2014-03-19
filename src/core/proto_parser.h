#ifndef _HPIO_PROTO_PARSER_
#define _HPIO_PROTO_PARSER_

#include "errno.h"
#include "hdr.h"
#include "ds/list.h"
#include "os/memory.h"
#include "os/epoll.h"
#include "bufio/bio.h"
#include "stats/modstat.h"
#include "transport/tcp/tcp.h"

enum {
    PP_RECV = 0,
    PP_SEND,
    PP_ERROR,
    PP_RTT,
    PP_RECONNECT,
    PP_MODSTAT_KEYRANGE,
};
extern const char *pp_modstat_item[PP_MODSTAT_KEYRANGE];
DEFINE_MODSTAT(pp, PP_MODSTAT_KEYRANGE);

typedef struct proto_parser {
    int sockfd;
    int64_t seqid;
    pp_modstat_t stat;
    struct bio in;
    struct bio out;
    pio_rgh_t rgh;
    epollevent_t et;
    struct io sock_ops;
    struct list_head pp_link;
} proto_parser_t;

#define list_for_each_pio_safe(pos, tmp, head)				\
    list_for_each_entry_safe(pos, tmp, head, proto_parser_t, pp_link)

void proto_parser_init(proto_parser_t *pp);
void proto_parser_destroy(proto_parser_t *pp);

static inline proto_parser_t *proto_parser_new() {
    proto_parser_t *pp = (proto_parser_t *)mem_zalloc(sizeof(*pp));
    if (pp)
	proto_parser_init(pp);
    return pp;
}

int proto_parser_ps_rgs(proto_parser_t *pp);
int proto_parser_at_rgs(proto_parser_t *pp);

static inline modstat_t *proto_parser_stat(proto_parser_t *pp) {
    return &pp->stat.self;
}

int proto_parser_bread(proto_parser_t *, pio_hdr_t *, char **, char **);
int proto_parser_bwrite(proto_parser_t *, const pio_hdr_t *, const char *, const char *);

static inline int proto_parser_one_ready(proto_parser_t *pp) {
    pio_hdr_t h = {};
    struct bio *b = &pp->in;
    if ((b->bsize >= sizeof(h)) && ({ bio_copy(b, (char *)&h, sizeof(h));
		b->bsize >= raw_pkg_size(&h);}))
	return true;
    return false;
}

static inline int proto_parser_prefetch(proto_parser_t *pp) {
    return bio_prefetch(&pp->in, &pp->sock_ops);
}

static inline int proto_parser_flush(proto_parser_t *pp) {
    while (!bio_empty(&pp->out))
	if (bio_flush(&pp->out, &pp->sock_ops) < 0)
	    return -1;
    return 0;
}


#endif
