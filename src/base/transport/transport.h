#ifndef _HPIO_TRANSPORT_
#define _HPIO_TRANSPORT_

#include "ds/list.h"

#define PIO_TCP -1
#define PIO_IPC -2
#define PIO_INPROC -3

#define PIO_LINGER 1
#define PIO_SNDBUF 2
#define PIO_RCVBUF 3
#define PIO_NONBLOCK 4
#define PIO_NODELAY 5
#define PIO_RCVTIMEO 6
#define PIO_SNDTIMEO 7
#define PIO_REUSEADDR 8
#define PIO_SOCKADDRLEN 4096

struct transport {
    const char *name;
    int proto;
    void (*global_init) (void);
    void (*close) (int fd);
    int (*bind) (const char *sock);
    int (*accept) (int fd);
    int (*connect) (const char *peer);
    int64_t (*read) (int fd, char *buff, int64_t size);
    int64_t (*write) (int fd, const char *buff, int64_t size);
    int (*setopt) (int fd, int opt, void *val, int vallen);
    int (*getopt) (int fd, int opt, void *val, int vallen);
    struct list_head item;
};

struct transport *transport_lookup(int proto);


#endif
