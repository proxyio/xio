#ifndef _HPIO_PROXYIO_
#define _HPIO_PROXYIO_

#include "errno.h"
#include "os/memory.h"
#include "io.h"
#include "bufio/bio.h"
#include "proto/phdr.h"
#include "net/socket.h"

typedef struct proxyio {
    int sockfd;
    int64_t seqid;
    struct bio in;
    struct bio out;
    pio_rgh_t rgh;
    struct io sock_ops;
} proxyio_t;

static inline int64_t proxyio_sock_read(struct io *sock, char *buff, int64_t sz) {
    proxyio_t *io = container_of(sock, proxyio_t, sock_ops);
    sz = sk_read(io->sockfd, buff, sz);
    //printf("pio read %d with sz %ld\n", io->sockfd, sz);
    return sz;
}

static inline int64_t proxyio_sock_write(struct io *sock, char *buff, int64_t sz) {
    proxyio_t *io = container_of(sock, proxyio_t, sock_ops);
    sz = sk_write(io->sockfd, buff, sz);
    //printf("pio write %d with sz %ld\n", io->sockfd, sz);
    return sz;
}


static inline void proxyio_init(proxyio_t *io) {
    ZERO(*io);
    bio_init(&io->in);
    bio_init(&io->out);
    io->sock_ops.read = proxyio_sock_read;
    io->sock_ops.write = proxyio_sock_write;
}

static inline void proxyio_destroy(proxyio_t *io) {
    bio_destroy(&io->in);
    bio_destroy(&io->out);
}

static inline proxyio_t *proxyio_new() {
    proxyio_t *io = (proxyio_t *)mem_zalloc(sizeof(*io));
    if (io)
	proxyio_init(io);
    return io;
}

int proxyio_ps_rgs(proxyio_t *io);
int proxyio_at_rgs(proxyio_t *io);

int proxyio_recv(proxyio_t *io, struct pio_hdr *h, char **data, char **rt);
int proxyio_send(proxyio_t *io,
		 const struct pio_hdr *h, const char *data, const char *rt);

static inline int proxyio_flush(proxyio_t *io) {
    while (!bio_empty(&io->out))
	if (bio_flush(&io->out, &io->sock_ops) < 0 && errno != EAGAIN)
	    return -1;
    return 0;
}


#endif
