#include <x/xsock.h>
#include "url_parse.h"

/* URL example : 
 * net    group@net://182.33.49.10
 * ipc    group@ipc://xxxxxxxxxxxxxxxxxx
 * inproc group@inproc://xxxxxxxxxxxxxxxxxxxx
 */
int url_parse_group(const char *url, char *buff, u32 size) {
    char *at = strchr(url, '@');;

    if (!at) {
	errno = EINVAL;
	return -1;
    }
    strncpy(buff, url, size <= at - url ? size : at - url);
    return 0;
}

int url_parse_pf(const char *url) {
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
    if (strncmp(at, "net", pf - at) == 0)
	return PF_NET;
    else if (strncmp(at, "ipc", pf - at) == 0)
	return PF_IPC;
    else if (strncmp(at, "inproc", pf - at) == 0)
	return PF_INPROC;
    errno = EINVAL;
    return -1;
}

int url_parse_sockaddr(const char *url, char *buff, u32 size) {
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

