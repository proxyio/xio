#ifndef _HPIO_ENDPOINT_
#define _HPIO_ENDPOINT_


void *xep_allocbuf(int size);
void xep_freebuf(void *buf);
void xep_copyhdr(void *src_buf, void *dst_buf);

int xep_open(int size);
void xep_close(int eid);
int xep_add_in(int eid, int xd);
int xep_add_out(int eid, int xd);
int xep_rm(int eid, int xd);

typedef void (*walkfn) (int eid, int xd, void *self);
int xep_walk(int eid, walkfn f, void *self);


#endif
