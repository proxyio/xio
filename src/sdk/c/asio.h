#ifndef _HPIO_ASIO_
#define _HPIO_ASIO_

#include "io.h"

typedef struct aspio aspio_t;

typedef struct aspio_cf {
    char host[HOSTNAME_MAX];
    char proxy[PROXYNAME_MAX];
    int max_cpus;
    int sockbuf;
    int max_trip_time;
} aspio_cf_t;

typedef struct resp_writer {
    int (*send_resp) (struct resp_writer *rw, const char *resp, uint32_t sz);
    int (*close) (struct resp_writer *rw);
} resp_writer_t;
typedef int (*request_come_f) (const char *req, uint32_t sz, resp_writer_t rw);
aspio_t *aspio_join_comsumer(aspio_cf_t *cf, request_come_f f);


typedef int (*response_back_f) (const char *resp, uint32_t sz, void *ctx);
aspio_t *aspio_join_producer(aspio_cf_t *cf, response_back_f f);
int as_producer_send(aspio_t *asio, const char *req, uint32_t sz, void *ctx, int to);


int aspio_start(aspio_t *asio);
int aspio_stop(aspio_t *asio);
void aspio_close(aspio_t *asio);


#endif
