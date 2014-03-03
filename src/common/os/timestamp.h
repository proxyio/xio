#ifndef _H_TIMESTAMP_
#define _H_TIMESTAMP_

#include <inttypes.h>

int64_t rt_mstime();
int64_t rt_ustime();
int64_t rt_nstime();
int rt_usleep(int64_t usec);

#endif
