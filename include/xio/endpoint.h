#ifndef _HPIO_ENDPOINT_
#define _HPIO_ENDPOINT_

#define XEPBUF_CLONEHDR  0x01
void *xep_allocbuf(int flags, int size, ...);
void xep_freebuf(void *buf);

#define XEP_PRODUCER 0
#define XEP_COMSUMER 1

int xep_open();
void xep_close(int eid);
int xep_add(int eid, int sfd);
int xep_rm(int eid, int sfd);
int xep_recv(int eid, char **xbuf);
int xep_send(int eid, char *xbuf);
int xep_pipeline(int receiver_eid, int dispatcher_eid);



#endif
