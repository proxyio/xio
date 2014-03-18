#ifndef _HPIO_IO_
#define _HPIO_IO_

#define PROXYNAME_MAX 128
#define HOSTNAME_MAX PROXYNAME_MAX
#define PIO_VERSION 0x1

#include <inttypes.h>
#include <sys/uio.h>

typedef int pio_t;

struct pio_vec {
    char *iov_base;
    uint32_t iov_len;
};

struct pio_msg {
    struct pio_vec sys_rt;
    int chunk;
    uint32_t flags;
    struct pio_vec vec[0];
};

struct pio_msg *alloc_pio_msg(int chunk);
void free_pio_msg(struct pio_msg *msg);
void free_pio_msg_and_vec(struct pio_msg *msg);

pio_t *pio_join_comsumer(const char *addr, const char *pyn);
pio_t *pio_join_producer(const char *addr, const char *pyn);
void pio_close(pio_t *io);

int pio_flush(pio_t *io);
int pio_recvmsg(pio_t *io, struct pio_msg **msg);
int pio_sendmsg(pio_t *io, const struct pio_msg *msg);


#endif
