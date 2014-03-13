#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "errno.h"
#include "proxyio.h"
#include "net/socket.h"


int proxyio_attach(proxyio_t *io, int sockfd, const pio_rgh_t *rgh) {
    memcpy(&io->rgh, rgh, sizeof(*rgh));
    io->sockfd = sockfd;
    return 0;
}

int proxyio_register(proxyio_t *io, const char *addr,
		     const char py[PROXYNAME_MAX], int client_type) {
    int64_t nbytes = 0, ret;

    io->rgh.type = client_type;
    uuid_generate(io->rgh.id);
    memcpy(io->rgh.proxyname, py, sizeof(py));
    if ((io->sockfd = sk_connect("tcp", "", addr)) < 0)
	return -1;
    while (nbytes < PIORGHLEN) {
	if ((ret = sk_write(io->sockfd, ((char *)&io->rgh) + nbytes,
			    PIORGHLEN - nbytes)) < 0 && errno != EAGAIN) {
	    close(io->sockfd);
	    return -1;
	}
	nbytes += ret;
    }
    return 0;
}

int proxyio_recv(comsumer_t *pp,
		 struct pio_hdr *h, char **data, char **rt) {
    proxyio_t *io = container_of(pp, proxyio_t, sockfd);
    struct bio *b = &io->b;

    if ((b->bsize >= sizeof(*h)) && !({
		bio_copy(b, (char *)h, sizeof(*h)); ph_validate(h);})) {
	errno = PIO_ECHKSUM;
	return -1;
    }
    while (b->bsize < sizeof(*h) || b->bsize < pio_pkg_size(h))
	if (bio_prefetch(b, &io->sock_ops) < 0 && errno != EAGAIN)
	    return -1;
    if (!(*data = (char *)mem_zalloc(h->size)))
	return -1;
    if (!(*rt = (char *)mem_zalloc(pio_rt_size(h)))) {
	mem_free(*data, h->size);
	*data = NULL;
	return -1;
    }
    bio_read(b, (char *)h, sizeof(*h));
    bio_read(b, *data, h->size);
    bio_read(b, *rt, pio_rt_size(h));
    return 0;
}


int proxyio_send(comsumer_t *pp, struct pio_hdr *h, char *data, char *rt) {
    proxyio_t *io = container_of(pp, proxyio_t, sockfd);

    if (ph_validate(h)) {
	errno = PIO_ECHKSUM;
	return -1;
    }
    ph_makechksum(h);
    bio_write(&io->b, (char *)h, sizeof(h));
    bio_write(&io->b, data, h->size);
    bio_write(&io->b, rt, pio_rt_size(h));
    while (!bio_empty(&io->b))
	if (bio_flush(&io->b, &io->sock_ops) < 0 && errno != EAGAIN)
	    return -1;
    return 0;
}

