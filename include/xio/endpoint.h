#ifndef _HPIO_ENDPOINT_
#define _HPIO_ENDPOINT_

#ifdef __cplusplus
extern "C" {
#endif

#define XEPBUF_CLONEHDR  0x01
char *xep_allocubuf(int flags, int size, ...);
void xep_freeubuf(char *buf);

#define XEP_PRODUCER 1
#define XEP_COMSUMER 2

int xep_open(int type);
void xep_close(int eid);
int xep_add(int eid, int sfd);
int xep_rm(int eid, int sfd);
int xep_recv(int eid, char **xbuf);
int xep_send(int eid, char *xbuf);
int xep_pipeline(int receiver_eid, int dispatcher_eid);

void efd_strace(int efd);
    
#ifdef __cplusplus
}
#endif

#endif
