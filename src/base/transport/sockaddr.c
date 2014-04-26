#include <base.h>
#include "transport.h"
#include "sockaddr.h"

/* SOCKADDR example : 
 * ipc    group@ipc://tmp/ipc.sock
 * net    group@net://182.33.49.10:8080
 * inproc group@inproc://inproc.sock
 */
int sockaddr_group(const char *url, char *buff, u32 size) {
    char *at = strchr(url, '@');;

    if (!at) {
	errno = EINVAL;
	return -1;
    }
    strncpy(buff, url, size <= at - url ? size : at - url);
    return 0;
}

int sockaddr_pf(const char *url) {
    int pf = 0;
    char *at = strchr(url, '@');;
    char *pfp = strstr(url, "://");;

    if (!pfp || at >= pfp) {
	errno = EINVAL;
	return -1;
    }
    if (at)
	++at;
    else
	at = (char *)url;
    pfp = strndup(at, pfp - at);
    pf |= strstr(pfp, "tcp") ? TP_TCP : 0;
    pf |= strstr(pfp, "ipc") ? TP_IPC : 0;
    pf |= strstr(pfp, "inproc") ? TP_INPROC : 0;
    if (!pf) {
	errno = EINVAL;
	return -1;
    }
    return pf;
}

int sockaddr_addr(const char *url, char *buff, u32 size) {
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
