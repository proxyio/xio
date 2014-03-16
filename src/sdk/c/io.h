#ifndef _HPIO_IO_
#define _HPIO_IO_

#define PROXYNAME_MAX 128
#define PIO_VERSION 0x1

#include <inttypes.h>

enum {
    PRODUCER = 2,
    COMSUMER = 3,
};

typedef int pio_t;
pio_t *pio_join(const char *addr, const char py[PROXYNAME_MAX], int type);
void pio_close(pio_t *io);

int producer_send_request(pio_t *io, const char *data, uint32_t size);
int producer_recv_response(pio_t *io, char **data, uint32_t *size);
int producer_psend_request(pio_t *io, const char *data, uint32_t size);
int producer_precv_response(pio_t *io, char **data, uint32_t *size);


int comsumer_send_response(pio_t *io, const char *data, uint32_t size,
			   const char *rt, uint32_t rt_size);
int comsumer_recv_request(pio_t *io, char **data, uint32_t *size,
			  char **rt, uint32_t *rt_size);
int comsumer_psend_response(pio_t *io, const char *data, uint32_t size,
			    const char *rt, uint32_t rt_size);
int comsumer_precv_request(pio_t *io, char **data, uint32_t *size,
			   char **rt, uint32_t *rt_size);



#endif
