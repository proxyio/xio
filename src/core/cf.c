#include <stdio.h>
#include "core.h"


struct cf default_cf = {
    .max_cpus = 2,
    .el_io_size = 10240,
    .el_wait_timeout = 1,
    .monitor_center = NULL,
};
