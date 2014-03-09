#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include "ds/slice.h"
#include "parser.h"

static int __pp_drop(struct pio_parser *pp, io_t *c, uint32_t size) {
    int ret = 0;
    uint32_t idx = 0, s = 0;
    char dropbuf[4096] = {};
    
    while (pp->recv_data_idx < size) {
	idx = pp->recv_data_idx % sizeof(dropbuf);
	if ((s = sizeof(dropbuf) - idx) > size - pp->recv_data_idx)
	    s = size - pp->recv_data_idx;
	if ((ret = c->read(c, dropbuf + idx, s)) < 0)
	    return -1;
	pp->recv_data_idx += ret;
    }
    pp->dropped_cnt++;
    errno = EAGAIN;
    pp_reset_recv(pp);
    return -1;
}

static int __pp_recv_hdr(struct pio_parser *pp, io_t *c, struct pio_hdr *hdr) {
    int64_t nbytes = 0;
    uint32_t size = PIOHDRLEN;

    while (pp->recv_hdr_idx < size) {
	nbytes = c->read(c, (char *)hdr + pp->recv_hdr_idx, size - pp->recv_hdr_idx);
	if (nbytes < 0)
	    return -1;
	pp->recv_hdr_idx += nbytes;
    }
    if (ph_validate(hdr) != true) {
	errno = EPIPE;
	return -1;
    }
    return 0;
}

static int __pp_recv_data(struct pio_parser *pp, io_t *c, int n, struct slice *s) {
    int64_t nbytes = 0;
    char *buffer = NULL;
    uint32_t cap = 0, size = 0;

    slice_bufcap(n, s, cap);
    while (pp->recv_data_idx < cap) {
	locate_slice_buffer(pp->recv_data_idx, s, buffer, size);
	if ((nbytes = c->read(c, buffer, size)) < 0)
	    return -1;
	pp->recv_data_idx += nbytes;
    }
    return 0;
}

static int __pp_send_hdr(struct pio_parser *pp, io_t *c, const struct pio_hdr *hdr) {
    int64_t nbytes = 0;
    uint32_t size = PIOHDRLEN;
    struct pio_hdr copyh = *hdr;

    ph_makechksum(&copyh);
    while (pp->send_hdr_idx < size) {
	nbytes = c->write(c, (char *)&copyh + pp->send_hdr_idx, size - pp->send_hdr_idx);
	if (nbytes < 0)
	    return -1;
	pp->send_hdr_idx += nbytes;
    }
    return 0;
}

static int __pp_send_data(struct pio_parser *pp, io_t *c, int n, struct slice *s) {
    int64_t nbytes = 0;
    char *buffer = NULL;
    uint32_t cap = 0, size = 0;

    slice_bufcap(n, s, cap);
    while (pp->send_data_idx < cap) {
	locate_slice_buffer(pp->send_data_idx, s, buffer, size);
	if ((nbytes = c->write(c, buffer, size)) < 0)
	    return -1;
	pp->send_data_idx += nbytes;
    }
    return 0;
}

int pp_send_rgh_async(struct pio_parser *pp, io_t *c) {
    int64_t nbytes = 0;
    uint32_t size = PIORGHLEN;
    pio_rgh_t *h = &pp->rgh;

    while (pp->send_rgh_idx < size) {
	nbytes = c->write(c, (char *)h + pp->send_rgh_idx, size - pp->send_rgh_idx);
	if (nbytes < 0)
	    return -1;
	pp->send_rgh_idx += nbytes;
    }
    return 0;
}

int pp_recv_rgh(struct pio_parser *pp, io_t *c) {
    int64_t nbytes = 0;
    uint32_t size = PIORGHLEN;
    pio_rgh_t *h = &pp->rgh;

    while (pp->recv_rgh_idx < size) {
	nbytes = c->read(c, (char *)h + pp->recv_rgh_idx, size - pp->recv_rgh_idx);
	if (nbytes < 0)
	    return -1;
	pp->recv_rgh_idx += nbytes;
    }
    return 0;
}


int pp_send_async(struct pio_parser *pp, io_t *c, pio_msg_t *msg) {
    struct slice s = {};

    if (__pp_send_hdr(pp, c, &msg->hdr) < 0)
	return -1;
    s.len = pio_msg_size(msg) - PIOMSGLEN;
    s.data = msg->data;
    if (__pp_send_data(pp, c, 1, &s) < 0)
	return -1;
    pp_reset_send(pp);
    return 0;
}

int pp_send(struct pio_parser *pp, io_t *c, pio_msg_t *msg) {
    int ret = 0;

    do {
	ret = pp_send_async(pp, c, msg);
    } while (ret < 0 && errno == EAGAIN);

    return ret;
}


int pp_recv(struct pio_parser *pp, io_t *c, pio_msg_t **msg, int reserve) {
    struct slice s = {};
    pio_msg_t *tmp = &pp->msg;

    if (__pp_recv_hdr(pp, c, &tmp->hdr) < 0)
	return -1;
    if (pp->package_size_max && tmp->hdr.size >= pp->package_size_max)
	return __pp_drop(pp, c, tmp->hdr.size);
    if (!pp->bufmsg && !(pp->bufmsg = pio_msg_clone_reserve(tmp, reserve))) {
	errno = EAGAIN;
	return -1;
    }
    s.len = pio_msg_size(pp->bufmsg) - PIOMSGLEN;
    s.data = pp->bufmsg->data;
    if (__pp_recv_data(pp, c, 1, &s) < 0)
	return -1;
    *msg = pp->bufmsg;
    pp->bufmsg = NULL;
    pp_reset_recv(pp);
    return 0;
}
