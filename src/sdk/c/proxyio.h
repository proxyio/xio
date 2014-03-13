#ifndef _HPIO_PROXYIO_
#define _HPIO_PROXYIO_

#include "os/malloc.h"
#include "io.h"
#include "bufio/bio.h"
#include "proto/phdr.h"
#include "net/socket.h"

typedef struct proxyio {
    int sockfd;
    int64_t seqid;
    struct bio b;
    pio_rgh_t rgh;
    struct io sock_ops;
} proxyio_t;

static inline int64_t proxyio_sock_read(struct io *sock, char *buff, int64_t sz) {
    proxyio_t *io = container_of(sock, proxyio_t, sock_ops);
    return sk_read(io->sockfd, buff, sz);
}

static inline int64_t proxyio_sock_write(struct io *sock, char *buff, int64_t sz) {
    proxyio_t *io = container_of(sock, proxyio_t, sock_ops);
    return sk_write(io->sockfd, buff, sz);
}


static inline void proxyio_init(proxyio_t *io) {
    ZERO(*io);
    bio_init(&io->b);
    io->sock_ops.read = proxyio_sock_read;
    io->sock_ops.write = proxyio_sock_write;
}

static inline proxyio_t *proxyio_new() {
    proxyio_t *io = (proxyio_t *)mem_zalloc(sizeof(*io));
    if (io)
	proxyio_init(io);
    return io;
}

int proxyio_attach(proxyio_t *io, int sockfd, const pio_rgh_t *rgh);
int proxyio_register(proxyio_t *io, const char *addr,
		     const char py[PROXYNAME_MAX], int client_type);
int proxyio_recv(comsumer_t *pp, struct pio_hdr *h, char **data, char **rt);
int proxyio_send(comsumer_t *pp, struct pio_hdr *h, char *data, char *rt);



#endif
