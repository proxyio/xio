#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "io.h"
#include "core/proto_parser.h"
#include "net/socket.h"

struct pio_msg *alloc_pio_msg(int chunk) {
    struct pio_msg *msg = (struct pio_msg *)
	mem_zalloc(sizeof(*msg) + chunk * sizeof(struct pio_vec));
    msg->chunk = chunk;
    return msg;
}

void free_pio_msg(struct pio_msg *msg) {
    if (msg->sys_rt.iov_len > 0)
	mem_free(msg->sys_rt.iov_base, msg->sys_rt.iov_len);
    mem_free(msg, sizeof(*msg) + msg->chunk * sizeof(struct pio_vec));
}

void free_pio_msg_and_vec(struct pio_msg *msg) {
    int chunks = msg->chunk;
    while (chunks-- > 0)
	if (msg->vec[chunks].iov_len > 0)
	    mem_free(msg->vec[chunks].iov_base, msg->vec[chunks].iov_len);
    free_pio_msg(msg);
}

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

extern int producer_send(pio_t *io,
			 const char *data,
			 uint32_t size,
			 int to);

extern int producer_psend(pio_t *io,
			  const char *data,
			  uint32_t size,
			  int to);

extern int producer_recv(pio_t *io,
			 char **data,
			 uint32_t *size);

extern int producer_precv(pio_t *io,
			  char **data,
			  uint32_t *size);

extern int comsumer_send(pio_t *io,
			 const char *data,
			 uint32_t size,
			 const char *rt,
			 uint32_t rt_size);

extern int comsumer_psend(pio_t *io,
			  const char *data,
			  uint32_t size,
			  const char *rt,
			  uint32_t rt_size);

extern int comsumer_recv(pio_t *io,
			 char **data,
			 uint32_t *size,
			 char **rt,
			 uint32_t *rt_size);

extern int comsumer_precv(pio_t *io,
			  char **data,
			  uint32_t *size,
			  char **rt,
			  uint32_t *rt_size);



int pio_recvmsg(pio_t *io, struct pio_msg **msg) {
    struct pio_msg *new = alloc_pio_msg(1);
    proto_parser_t *pp = container_of(io, proto_parser_t, sockfd);
    if (!new)
	return -1;
    switch (pp->rgh.type) {
    case PIO_SNDER:
	if (comsumer_recv(io, &new->vec[0].iov_base, &new->vec[0].iov_len,
			  &new->sys_rt.iov_base, &new->sys_rt.iov_len) == 0)
	    return ({*msg = new; 0;});
	break;
    case PIO_RCVER:
	if (producer_recv(io, &new->vec[0].iov_base, &new->vec[0].iov_len) == 0)
	    return ({*msg = new; 0;});
	break;
    default:
	errno = EINVAL;
    }
    free_pio_msg(new);
    return -1;
}

int pio_sendmsg(pio_t *io, const struct pio_msg *msg) {
    proto_parser_t *pp = container_of(io, proto_parser_t, sockfd);
    switch (pp->rgh.type) {
    case PIO_SNDER:
	return comsumer_psend(io, msg->vec[0].iov_base, msg->vec[0].iov_len,
			      msg->sys_rt.iov_base, msg->sys_rt.iov_len);
    case PIO_RCVER:
	return producer_psend(io, msg->vec[0].iov_base, msg->vec[0].iov_len, 0);
    }
    errno = EINVAL;
    return -1;
}
