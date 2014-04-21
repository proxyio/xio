#ifndef _HPIO_XURL_
#define _HPIO_XURL_

/* XURL example : 
 * ipc    group@ipc://tmp/ipc.sock
 * net    group@net://182.33.49.10
 * inproc group@inproc://inproc.sock
 */
int xurl_group(const char *url, char *buff, u32 size);

int xurl_pf(const char *url);

int xurl_sockaddr(const char *url, char *buff, u32 size);






#endif
