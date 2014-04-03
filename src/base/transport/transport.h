#ifndef _HPIO_TRANSPORT_
#define _HPIO_TRANSPORT_

#include "ds/list.h"

#define TP_TCP 1
#define TP_IPC 2
#define TP_MOCK_INPROC 4

#define TP_LINGER 1
#define TP_SNDBUF 2
#define TP_RCVBUF 3
#define TP_NOBLOCK 4
#define TP_NODELAY 5
#define TP_RCVTIMEO 6
#define TP_SNDTIMEO 7
#define TP_REUSEADDR 8
#define TP_SOCKADDRLEN 128

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
