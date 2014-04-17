#include <x/xsock.h>
#include "ep_url.h"

/* URL example : 
 * net    group@net://182.33.49.10
 * ipc    group@ipc://xxxxxxxxxxxxxxxxxx
 * inproc group@inproc://xxxxxxxxxxxxxxxxxxxx
 */
static int url_parse_group(const char *url, char *buff, u32 size) {
    char *at = strchr(url, '@');;

    if (!at) {
	errno = EINVAL;
	return -1;
    }
    strncpy(buff, url, size <= at - url ? size : at - url);
    return 0;
}

static int url_parse_pf(const char *url) {
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

static int url_parse_sockaddr(const char *url, char *buff, u32 size) {
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

struct fd *fd_open(const char *url) {
    struct fd *f = fd_new();
    int pf = url_parse_pf(url);
    char sock[URLNAME_MAX] = {};

    BUG_ON(!f);
    if (pf < 0 || url_parse_group(url, f->st.group, URLNAME_MAX) < 0
	|| url_parse_sockaddr(url, sock, URLNAME_MAX) < 0) {
	fd_free(f);
	errno = EINVAL;
	return 0;
    }
    if ((f->xd = xconnect(pf, sock)) < 0) {
	fd_free(f);
	errno = EAGAIN;
	return 0;
    }
    return f;
}

struct fd *fd_listen(const char *url) {
    struct fd *f = fd_new();
    int pf = url_parse_pf(url);
    char sock[URLNAME_MAX] = {};

    BUG_ON(!f);
    if (pf < 0 || url_parse_group(url, f->st.group, URLNAME_MAX) < 0
	|| url_parse_sockaddr(url, sock, URLNAME_MAX) < 0) {
	fd_free(f);
	errno = EINVAL;
	return 0;
    }
    if ((f->xd = xlisten(pf, sock)) < 0) {
	fd_free(f);
	errno = EAGAIN;
	return 0;
    }
    return f;
}


struct fd *fd_accept(struct fd *f) {
    struct fd *newf = fd_new();

    BUG_ON(!newf);
    if ((newf->xd = xaccept(f->xd)) < 0) {
	fd_free(newf);
	return 0;
    }
    strcpy(newf->st.group, f->st.group);
    return newf;
}
