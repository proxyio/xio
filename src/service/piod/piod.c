#include <stdio.h>
#include "core/accepter.h"

extern int64_t deadline;
extern char *proxyhost;
extern char *proxyname;
extern int getoption(int argc, char* argv[], struct cf *cf);

int main(int argc, char **argv) {
    acp_t acp = {};
    struct cf cf = default_cf;
    
    if (getoption(argc, argv, &cf) < 0)
	return -1;
    acp_init(&acp, &cf);
    acp_proxy(&acp, proxyname);
    acp_listen(&acp, proxyhost);
    acp_start(&acp);
    while (rt_mstime() < deadline)
	sleep(1);
    acp_stop(&acp);
    return 0;
}

