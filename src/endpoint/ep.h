#ifndef _HPIO_ENDPOINT_
#define _HPIO_ENDPOINT_

#ifdef __cplusplus
extern "C" {
#endif

#define XEPUBUF_CLONEHDR  0x01
    
char *xep_allocubuf(int size);
void xep_freeubuf(char *ubuf);
uint32_t xep_ubuflen(char *ubuf);


#define XEP_PRODUCER 1
#define XEP_COMSUMER 2

int xep_open(int eptype);
void xep_close(int eid);
int xep_add(int eid, int xsockfd);
int xep_rm(int eid, int xsockfd);
int xep_recv(int eid, char **ubuf);
int xep_send(int eid, char *ubuf);

#define XEP_DISPATCHTO 1
int xep_setopt(int eid, int opt, void *optval, int optlen);
int xep_getopt(int eid, int opt, void *optval, int *optlen);

#ifdef __cplusplus
}
#endif

#endif
