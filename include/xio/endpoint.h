#ifndef _HPIO_ENDPOINT_
#define _HPIO_ENDPOINT_

#define XEPBUF_CLONEHDR  0x01
void *xep_allocbuf(int flags, int size, ...);
void xep_freebuf(void *buf);

#define XIO_PRODUCER 1
#define XIO_COMSUMER 2

int xep_open();
void xep_close(int eid);
int xep_add(int eid, int sfd);
int xep_rm(int eid, int sfd);
int xep_recv(int eid, void **xbuf);
int xep_send(int eid, void *xbuf);
int xep_pipeline(int receiver_eid, int dispatcher_eid);



#endif
