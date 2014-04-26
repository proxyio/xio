#include <stdio.h>
#include "xgb.h"

int xcpu_alloc() {
    int cpu_no;

    mutex_lock(&xgb.lock);
    BUG_ON(xgb.ncpus >= XSOCK_MAX_CPUS);
    cpu_no = xgb.cpu_unused[xgb.ncpus++];
    mutex_unlock(&xgb.lock);
    return cpu_no;
}

int xcpu_choosed(int xd) {
    return xd % xgb.ncpus;
}

void xcpu_free(int cpu_no) {
    mutex_lock(&xgb.lock);
    xgb.cpu_unused[--xgb.ncpus] = cpu_no;
    mutex_unlock(&xgb.lock);
}

struct xcpu *xcpuget(int cpu_no) {
    return &xgb.cpus[cpu_no];
}


