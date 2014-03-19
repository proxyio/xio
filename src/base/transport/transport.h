#ifndef _HPIO_TRANSPORT_
#define _HPIO_TRANSPORT_

enum {
    PIO_NONBLOCK = 1,
    PIO_NODELAY,
    PIO_RECVTIMEOUT,
    PIO_SENDTIMEOUT,
    PIO_REUSEADDR,
};

struct transport {
    const char *name;
    int (*recv) (struct transport *tp, char *buff, int sz);
    int (*send) (struct transport *tp, const char *buff, int sz);
    int (*setopt) (struct transport *tp, int opt, const void *val, int len);
    int (*getopt) (struct transport *tp, int opt, void *val, int *len);
};


struct transport *tp_bind(const char *addr);
struct transport *tp_connect(const char *addr);
void tp_close(struct transport *tp);


#endif
