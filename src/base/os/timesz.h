#ifndef _HPIO_TIMESTAMP_
#define _HPIO_TIMESTAMP_

#include <inttypes.h>
#include "base.h"

i64 rt_mstime();
i64 rt_ustime();
i64 rt_nstime();
int rt_usleep(i64 usec);

#endif
