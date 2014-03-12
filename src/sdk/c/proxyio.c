#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "proxyio.h"
#include "net/socket.h"


int proxyio_register(proxyio_t *io, const char *addr,
		     const char grp[GRPNAME_MAX], int client_type) {
    int64_t nbytes = 0, ret;

    io->rgh.type = client_type;
    uuid_generate(io->rgh.id);
    memcpy(io->rgh.grpname, grp, sizeof(grp));
    if ((io->sockfd = sk_connect("tcp", "", addr)) < 0)
	return -1;
    while (nbytes < PIORGHLEN) {
	if ((ret = sk_write(io->sockfd, ((char *)&io->rgh) + nbytes,
			    PIORGHLEN - nbytes)) < 0)
	    break;
    }
    if (nbytes != PIORGHLEN) {
	close(io->sockfd);
	return -1;
    }
    return 0;
}
