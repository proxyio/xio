#ifndef _HPIO_IO_
#define _HPIO_IO_

#define PROXYNAME_MAX 128
#define PIO_VERSION 0x1

typedef int comsumer_t;
typedef int producer_t;

producer_t *producer_new(const char *addr, const char py[PROXYNAME_MAX]);
void producer_destroy(producer_t *io);
int producer_send_request(producer_t *io, const char *data, uint32_t size);
int producer_recv_response(producer_t *io, char **data, uint32_t *size);
int producer_psend_request(producer_t *io, const char *data, uint32_t size);
int producer_precv_response(producer_t *io, char **data, uint32_t *size);


comsumer_t *comsumer_new(const char *addr, const char py[PROXYNAME_MAX]);
void comsumer_destroy(comsumer_t *io);

int comsumer_send_response(comsumer_t *io, const char *data, uint32_t size,
			   const char *rt, uint32_t rt_size);
int comsumer_recv_request(comsumer_t *io, char **data, uint32_t *size,
			  char **rt, uint32_t *rt_size);

int comsumer_psend_response(comsumer_t *io, const char *data, uint32_t size,
			    const char *rt, uint32_t rt_size);
int comsumer_precv_request(comsumer_t *io, char **data, uint32_t *size,
			   char **rt, uint32_t *rt_size);



#endif
