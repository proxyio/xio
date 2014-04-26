#ifndef _HPIO_XSOCKADDR_
#define _HPIO_XSOCKADDR_

/* XSOCKADDR example : 
 * ipc    group@ipc://tmp/ipc.sock
 * net    group@net://182.33.49.10
 * inproc group@inproc://inproc.sock
 */
int xsockaddr_group(const char *url, char *buff, u32 size);

int xsockaddr_pf(const char *url);

int xsockaddr_addr(const char *url, char *buff, u32 size);






#endif
