#ifndef _HPIO_PIPELINE_
#define _HPIO_PIPELINE_

void *xpipeline_allocbuf(int flags, int size, ...);
void xpipeline_freebuf(void *buf);

struct xpipeline {
    int in;
    int out;
};

int xpipeline_open(struct xpipeline *pp);
void xpipeline_close(struct xpipeline *pp);
int xpipeline_add(int xpipefd, int xsockfd);
int xpipeline_rm(int xpipefd, int xsockfd);


#endif
