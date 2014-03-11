#ifndef _HPIO_CAPI_
#define _HPIO_CAPI_

#define GRPNAME_MAX 128

typedef void * pio_producer_t;
typedef void * pio_comsumer_t;

pio_producer_t producer_new(const char *addr, const char grp[GRPNAME_MAX]);
void producer_destroy(pio_producer_t pp);
pio_comsumer_t comsumer_new(const char *addr, const char grp[GRPNAME_MAX]);
void comsumer_destroy(pio_comsumer_t pc);





#endif
