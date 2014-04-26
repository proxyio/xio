#include <base.h>
#include "transport.h"
#include "sockaddr.h"

/* SOCKADDR example : 
 * ipc    group@ipc://tmp/ipc.sock
 * net    group@net://182.33.49.10:8080
 * inproc group@inproc://inproc.sock
 */
int xsockaddr_group(const char *url, char *buff, u32 size) {
    char *at = strchr(url, '@');;

    if (!at) {
	errno = EINVAL;
	return -1;
    }
    strncpy(buff, url, size <= at - url ? size : at - url);
    return 0;
}

int xsockaddr_pf(const char *url) {
    char *at = strchr(url, '@');;
    char *pf = strstr(url, "://");;

    if (!pf || at >= pf) {
	errno = EINVAL;
	return -1;
    }
    if (at)
	++at;
    else
	at = (char *)url;
    if (strncmp(at, "tcp", pf - at) == 0)
	return TP_TCP;
    else if (strncmp(at, "ipc", pf - at) == 0)
	return TP_IPC;
    else if (strncmp(at, "inproc", pf - at) == 0)
	return TP_INPROC;
    errno = EINVAL;
    return -1;
}

int xsockaddr_addr(const char *url, char *buff, u32 size) {
    char *tok = "://";
    char *sock = strstr(url, tok);

    if (!sock) {
	errno = EINVAL;
	return -1;
    }
    sock += strlen(tok);
    strncpy(buff, sock, size);
    return 0;
}
