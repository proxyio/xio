#ifndef _HPIO_ASIO_
#define _HPIO_ASIO_

#include "io.h"

typedef struct apio apio_t;

typedef struct apio_cf {
    char host[HOSTNAME_MAX];
    char proxy[PROXYNAME_MAX];
    int max_cpus;
    int sockbuf;
    int max_trip_time;
} apio_cf_t;

typedef struct replyer {
    int (*send_resp) (struct replyer *ry, const char *resp, uint32_t sz);
    int (*close) (struct replyer *ry);
} replyer_t;
typedef int (*request_come_f) (const char *req, uint32_t sz, replyer_t ry);
apio_t *apio_join_comsumer(apio_cf_t *cf, request_come_f f);


typedef int (*response_back_f) (const char *resp, uint32_t sz, void *ctx);
apio_t *apio_join_producer(apio_cf_t *cf, response_back_f f);
int as_producer_send(apio_t *asio, const char *req, uint32_t sz, void *ctx, int to);


int apio_start(apio_t *asio);
int apio_stop(apio_t *asio);
void apio_close(apio_t *asio);


#endif
