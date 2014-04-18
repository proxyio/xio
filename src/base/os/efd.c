#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <base.h>
#include "efd.h"

int efd_init(struct efd *self) {
    int rc;
    int flags;
    int p[2];

    if ((rc = pipe(p)) < 0)
	return -1;
    self->r = p[0];
    self->w = p[1];

    flags = fcntl(self->r, F_GETFL, 0);
    if (flags == -1)
        flags = 0;
    rc = fcntl(self->r, F_SETFL, flags | O_NONBLOCK);
    return 0;
}

void efd_destroy(struct efd *self)
{
    close(self->r);
    close(self->w);
}

void efd_signal(struct efd *self)
{
    ssize_t nbytes;
    char c = 94;

    nbytes = write(self->w, &c, 1);
}

void efd_unsignal(struct efd *self)
{
    ssize_t nbytes;
    u8 buf[128];

    while(1) {
        nbytes = read(self->r, buf, sizeof(buf));
        if (nbytes < 0 && errno == EAGAIN)
            nbytes = 0;
        BUG_ON(nbytes < 0);
        if ((size_t) nbytes < sizeof(buf))
            break;
    }
}

