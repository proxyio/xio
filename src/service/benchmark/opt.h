#ifndef _HPIO_BCOPT_
#define _HPIO_BCOPT_

#include <inttypes.h>

enum {
    PINGPONG = 0,
    EXCEPTION,
};

struct bc_opt {
    int64_t deadline;
    int comsumer_num;
    int producer_num;
    char *host;
    char *proxyname;
    int mode;
};

int getoption(int argc, char *argv[], struct bc_opt *cf);

#endif
