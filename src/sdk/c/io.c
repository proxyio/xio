#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "io.h"
#include "core/proto_parser.h"
#include "net/socket.h"

pio_t *pio_join(const char *addr, const char *pyn, int type) {
    proto_parser_t *pp = proto_parser_new();

    proto_parser_init(pp);
    if ((pp->sockfd = sk_connect("tcp", "", addr)) < 0)
	goto RGS_ERROR;
    sk_setopt(pp->sockfd, SK_NONBLOCK, true);
    pp->rgh.type = (type == COMSUMER) ? PIO_SNDER : PIO_RCVER;
    uuid_generate(pp->rgh.id);
    memcpy(pp->rgh.proxyname, pyn, PROXYNAME_MAX);
    if (proto_parser_at_rgs(pp) < 0 && errno != EAGAIN)
	goto RGS_ERROR;
    while (!bio_empty(&pp->out))
	if (bio_flush(&pp->out, &pp->sock_ops) < 0)
	    goto RGS_ERROR;
    return &pp->sockfd;
 RGS_ERROR:
    proto_parser_destroy(pp);
    mem_free(pp, sizeof(*pp));
    return NULL;
}


void pio_close(pio_t *io) {
    proto_parser_t *pp = container_of(io, proto_parser_t, sockfd);
    close(pp->sockfd);
    proto_parser_destroy(pp);
    mem_free(pp, sizeof(*pp));
}
