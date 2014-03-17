#include <stdio.h>
#include "core/core.h"

static char __usage[] = "\n\
NAME\n\
    pio - a high throughput, low latency network communication middleware\n\
\n\
SYNOPSIS\n\
    pio -r time -w threads -m host\n\
\n\
OPTIONS\n\
    -r service running time. default forever\n		\
    -w threads. backend threadpool workers number\n\n\
    -m monitor_center host = ip + port\n\
\n\
EXAMPLE:\n\
    pio -r 60 -w 20\n\n";


static inline void usage() {
    system("clear");
    printf("%s", __usage);
}

int64_t deadline = 0;
char *proxyhost;

int getoption(int argc, char* argv[], struct cf *cf) {
    int rc;

    while ( (rc = getopt(argc, argv, "r:w:m:p:h:z")) != -1 ) {
        switch(rc) {
	case 'r':
	    deadline = rt_mstime() + atoi(optarg) * 1E3;
	    break;
        case 'w':
	    cf->max_cpus = atoi(optarg);
            break;
	case 'm':
	    cf->monitor_center = optarg;
	    break;
        case 'h':
	    proxyhost = optarg;
	    break;
	default:
	    usage();
	    return -1;
        }
    }
    return 0;
}

