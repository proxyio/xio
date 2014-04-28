#include <stdio.h>
#include "pipe_struct.h"


void xpipeline_close(struct xpipeline *pp) {
    pfd_free(pp->in);
    pfd_free(pp->out);
}
