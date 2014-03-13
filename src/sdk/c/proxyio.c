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
