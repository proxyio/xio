#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <php.h>
#include <xio/socket.h>
#include <xio/poll.h>
#include <xio/sp.h>
#include <xio/sp_reqrep.h>
#include "../../src/utils/base.h"
#include "php_xio.h"

PHP_FUNCTION(xallocubuf) {
    int rc;
    char *str;
    int str_sz;
    char *ubuf;
    
    rc = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_sz);
    if (rc == FAILURE)
	return;
    ubuf = xallocubuf(str_sz);
    memcpy(ubuf, str, str_sz);
    RETURN_LONG((long)ubuf);
}

PHP_FUNCTION(xfreeubuf) {
    int rc;
    char *ubuf = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &ubuf);
    if (rc == FAILURE)
	return;
    xfreeubuf(ubuf);
    RETURN_NULL();
}

PHP_FUNCTION(xubuflen) {
    int rc;
    char *ubuf = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &ubuf);
    if (rc == FAILURE)
	return;
    RETURN_LONG(xubuflen(ubuf));
}


PHP_FUNCTION(xsocket) {
    int rc;
    int pf = 0, socktype = 0;
    
    rc = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ii", &pf, &socktype);
    if (rc == FAILURE)
	return;
    RETURN_LONG(xsocket(pf, socktype));
}

PHP_FUNCTION(xbind) {
    int rc;
    int fd = 0;
    const char *sockaddr = 0;
    int socklen = 0;
    
    rc = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "is", &fd, &sockaddr,
			       &socklen);
    if (rc == FAILURE)
	return;
    RETURN_LONG(xbind(fd, sockaddr));
}

PHP_FUNCTION(xaccept) {
    int rc;
    int fd = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "i", &fd);
    if (rc == FAILURE)
	return;
    RETURN_LONG(xaccept(fd));
}


PHP_FUNCTION(xlisten) {
    int rc;
    const char *sockaddr = 0;
    int socklen = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &sockaddr, &socklen);
    if (rc == FAILURE)
	return;
    RETURN_LONG(xlisten(sockaddr));
}


PHP_FUNCTION(xconnect) {
    int rc;
    const char *sockaddr = 0;
    int socklen = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &sockaddr, &socklen);
    if (rc == FAILURE)
	return;
    RETURN_LONG(xconnect(sockaddr));
}


PHP_FUNCTION(xrecv) {
    int rc;
    int fd = 0;
    char *ubuf = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "i", &fd);
    if (rc == FAILURE)
	return;
    xrecv(fd, &ubuf);
    RETURN_LONG((long)ubuf);
}


PHP_FUNCTION(xsend) {
    int rc;
    int fd = 0;
    char *ubuf = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "il", &fd, &ubuf);
    if (rc == FAILURE)
	return;
    RETURN_LONG(xsend(fd, ubuf));
}

PHP_FUNCTION(xclose) {
    int rc;
    int fd = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "i", &fd);
    if (rc == FAILURE)
	return;
    RETURN_LONG(xclose(fd));
}

PHP_FUNCTION(xsetopt) {
    RETURN_NULL();
}

PHP_FUNCTION(xgetopt) {
    RETURN_NULL();
}

PHP_FUNCTION(sp_endpoint) {
    int rc;
    int eid = 0;
    int sp_family = 0;
    int sp_type = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS(), "ii", &sp_family, &sp_type);
    if (rc == FAILURE)
	return;
    eid = sp_endpoint(sp_family, sp_type);
    RETURN_LONG(eid);
}

PHP_FUNCTION(sp_close) {
    int rc;
    int eid = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS(), "i", &eid);
    if (rc == FAILURE)
	return;
    rc = sp_close(eid);
    RETURN_LONG(rc);
}

PHP_FUNCTION(sp_send) {
    int rc;
    int eid = 0;
    char *ubuf = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "il", &eid, &ubuf);
    if (rc == FAILURE)
	return;
    RETURN_LONG(sp_send(eid, ubuf));
}

PHP_FUNCTION(sp_recv) {
    int rc;
    int eid = 0;
    char *ubuf = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "i", &eid);
    if (rc == FAILURE)
	return;
    sp_recv(eid, &ubuf);
    RETURN_LONG((long)ubuf);
}

PHP_FUNCTION(sp_listen) {
    int rc;
    int eid = 0;
    const char *sockaddr = 0;
    int socklen = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "is", &eid, &sockaddr,
			       &socklen);
    if (rc == FAILURE)
	return;
    RETURN_LONG(sp_listen(eid, sockaddr));
}

PHP_FUNCTION(sp_connect) {
    int rc;
    int eid = 0;
    const char *sockaddr = 0;
    int socklen = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "is", &eid, &sockaddr,
			       &socklen);
    if (rc == FAILURE)
	return;
    RETURN_LONG(sp_connect(eid, sockaddr));
}

PHP_FUNCTION(sp_setopt) {
    RETURN_NULL();
}

PHP_FUNCTION(sp_getopt) {
    RETURN_NULL();
}

static function_entry xio_functions[] = {
    PHP_FE(xallocubuf,   0)
    PHP_FE(xfreeubuf,    0)
    PHP_FE(xubuflen,     0)
    PHP_FE(xsocket,      0)
    PHP_FE(xclose,       0)
    PHP_FE(xbind,        0)
    PHP_FE(xaccept,      0)
    PHP_FE(xlisten,      0)
    PHP_FE(xconnect,     0)
    PHP_FE(xrecv,        0)
    PHP_FE(xsend,        0)
    PHP_FE(xsetopt,      0)
    PHP_FE(xgetopt,      0)

    PHP_FE(sp_endpoint,  0)
    PHP_FE(sp_close,     0)
    PHP_FE(sp_send,      0)
    PHP_FE(sp_recv,      0)
    PHP_FE(sp_listen,    0)
    PHP_FE(sp_connect,   0)
    PHP_FE(sp_setopt,    0)
    PHP_FE(sp_getopt,    0)
    {0, 0, 0}
};

struct xsymbol {
    const char name[32];
    int value;
};

static struct xsymbol const_symbols[] = {
    {"XPOLLIN",      XPOLLIN},
    {"XPOLLOUT",     XPOLLOUT},
    {"XPOLLERR",     XPOLLERR},

    {"XPOLL_ADD",    XPOLL_ADD},
    {"XPOLL_DEL",    XPOLL_DEL},
    {"XPOLL_MOD",    XPOLL_MOD},

    {"XPF_TCP",      XPF_TCP},
    {"XPF_IPC",      XPF_IPC},
    {"XPF_INPROC",   XPF_INPROC},
    {"XLISTENER",    XLISTENER},
    {"XCONNECTOR",   XCONNECTOR},
    {"XSOCKADDRLEN", XSOCKADDRLEN},

    {"XL_SOCKET",    XL_SOCKET},
    {"XNOBLOCK",     XNOBLOCK},
    {"XSNDWIN",      XSNDWIN},
    {"XRCVWIN",      XRCVWIN},
    {"XSNDBUF",      XSNDBUF},
    {"XRCVBUF",      XRCVBUF},
    {"XLINGER",      XLINGER},
    {"XSNDTIMEO",    XSNDTIMEO},
    {"XRCVTIMEO",    XRCVTIMEO},
    {"XRECONNECT",   XRECONNECT},
    {"XSOCKTYPE",    XSOCKTYPE},
    {"XSOCKPROTO",   XSOCKPROTO},
    {"XTRACEDEBUG",  XTRACEDEBUG},

    {"SP_REQREP",    SP_REQREP},
    {"SP_BUS",       SP_BUS},
    {"SP_PUBSUB",    SP_PUBSUB},

    {"SP_REQ",       SP_REQ},
    {"SP_REP",       SP_REP},
    {"SP_PROXY",     SP_PROXY},
};

PHP_MINIT_FUNCTION(xio) {
    int i;
    struct xsymbol *sb;

    assert (sizeof(long) == sizeof(void *));

    for (i = 0; i < NELEM(const_symbols, struct xsymbol); i++) {
	sb = &const_symbols[i];
	REGISTER_MAIN_LONG_CONSTANT(sb->name, sb->value, CONST_CS | CONST_PERSISTENT);
    }
}

zend_module_entry xio_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_XIO_EXTNAME,
    xio_functions,
    PHP_MINIT(xio),
    NULL,
    NULL,
    NULL,
    NULL,
#if ZEND_MODULE_API_NO >= 20010901
    PHP_XIO_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_XIO
ZEND_GET_MODULE(xio)
#endif

