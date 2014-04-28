#include <stdio.h>
#include "pipe_struct.h"


int xpipeline_open(struct xpipeline *pp) {
    int in, out;

    if ((in = pfd_alloc()) < 0) {
	errno = EMFILE;
	return -1;
    }
    if ((out = pfd_alloc()) < 0) {
	pfd_free(in);
	errno = EMFILE;
	return -1;
    }
    pp->in = in;
    pp->out = out;
    return 0;
}
