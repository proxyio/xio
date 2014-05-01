#ifndef _HPIO_ENDPOINT_
#define _HPIO_ENDPOINT_

#ifdef __cplusplus
extern "C" {
#endif

#define XEPUBUF_CLONEHDR  0x01
char *xep_allocubuf(int flags, int size, ...);
void xep_freeubuf(char *ubuf);

#define XEP_PRODUCER 1
#define XEP_COMSUMER 2

int xep_open(int type);
void xep_close(int eid);
int xep_add(int eid, int sfd);
int xep_rm(int eid, int sfd);
int xep_recv(int eid, char **ubuf);
int xep_send(int eid, char *ubuf);
int xep_proxy(int front_eid, int backend_eid);
    
#ifdef __cplusplus
}
#endif

#endif
