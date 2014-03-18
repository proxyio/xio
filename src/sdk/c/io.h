#ifndef _HPIO_IO_
#define _HPIO_IO_

#define PROXYNAME_MAX 128
#define HOSTNAME_MAX PROXYNAME_MAX
#define PIO_VERSION 0x1

#include <inttypes.h>
#include <sys/uio.h>

typedef int pio_t;

struct pvec {
    char *iov_base;
    uint32_t iov_len;
};

typedef struct pmsg {
    struct pvec sys_rt;
    int chunk;
    uint32_t flags;
    struct pvec vec[0];
} pmsg_t;

pmsg_t *alloc_pio_msg(int chunk);
void free_pio_msg(pmsg_t *msg);
void free_pio_msg_and_vec(pmsg_t *msg);

pio_t *pio_join_comsumer(const char *addr, const char *pyn);
pio_t *pio_join_producer(const char *addr, const char *pyn);
void pio_close(pio_t *io);

int pio_flush(pio_t *io);
int pio_recvmsg(pio_t *io, pmsg_t **msg);
int pio_sendmsg(pio_t *io, const pmsg_t *msg);


#endif
