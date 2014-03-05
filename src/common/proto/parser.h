#ifndef _H_PIO_PROTO_PARSER_
#define _H_PIO_PROTO_PARSER_

#include "base.h"
#include "pio.h"
#include "net/socket.h"

struct pio_parser {
    uint64_t dropped_cnt;
    uint32_t package_size_max;
    uint32_t recv_hdr_index;
    uint32_t recv_data_index;
    uint32_t send_hdr_index;
    uint32_t send_data_index;
    pio_msg_t msg, *bufmsg;

    int (*send_async) (struct pio_parser *pp, conn_t *c, pio_msg_t *msg);
    int (*send) (struct pio_parser *pp, conn_t *c, pio_msg_t *msg);
    int (*recv) (struct pio_parser *pp, conn_t *c, pio_msg_t **msg, int reserve);
};


int pp_send_async(struct pio_parser *pp, conn_t *c, pio_msg_t *msg);
int pp_send(struct pio_parser *pp, conn_t *c, pio_msg_t *msg);
int pp_recv(struct pio_parser *pp, conn_t *c, pio_msg_t **msg, int reserve);

static inline void pp_init(struct pio_parser *pp, uint32_t max) {
    ZERO(*pp);
    pp->package_size_max = max;
    pp->send_async = pp_send_async;
    pp->send = pp_send;
    pp->recv = pp_recv;
}

static inline void pp_destroy(struct pio_parser *pp) {
    if (pp->bufmsg)
	mem_free((char *)pp->bufmsg, pio_msg_size(pp->bufmsg));
    pp->bufmsg = NULL;
}

static inline void pp_reset_recv(struct pio_parser *pp) {
    if (pp->bufmsg)
	mem_free((char *)pp->bufmsg, pio_msg_size(pp->bufmsg));
    pp->bufmsg = NULL;
    pp->recv_hdr_index = pp->recv_data_index = 0;
}

static inline void pp_reset_send(struct pio_parser *pp) {
    pp->send_hdr_index = pp->send_data_index = 0;
}

#endif
