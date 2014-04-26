#ifndef _HPIO_SOCKADDR_
#define _HPIO_SOCKADDR_

/* SOCKADDR example : 
 * ipc    group@ipc://tmp/ipc.sock
 * net    group@net://182.33.49.10:8080
 * inproc group@inproc://inproc.sock
 */
int sockaddr_group(const char *url, char *buff, u32 size);

int sockaddr_pf(const char *url);

int sockaddr_addr(const char *url, char *buff, u32 size);






#endif
