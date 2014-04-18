#ifndef _HPIO_EVENTFD_
#define _HPIO_EVENTFD_

struct efd {
    int r;
    int w;
};

int efd_init(struct efd *self);

void efd_destroy(struct efd *self);

void efd_signal(struct efd *self);

void efd_unsignal(struct efd *self);



#endif
