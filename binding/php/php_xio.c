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

// Definition for the message object
typedef struct {
    zend_object zo;
    char *ubuf;
} php_xmessage_object;

PHP_METHOD(xmessage, __construct) {
    int rc;
    php_xmessage_object *o = 0;
    char *str = 0;
    int str_len = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS(), "s", &str, &str_len);
    if (rc == FAILURE)
	return;
    o = (php_xmessage_object *) zend_object_store_get_object(getThis());
    assert (o->ubuf = xallocubuf(str_len));
    memcpy(o->ubuf, str, str_len);
}

PHP_METHOD(xmessage, __destruct) {
    php_xmessage_object *o = 0;

    o = (php_xmessage_object *) zend_object_store_get_object(getThis());
    if (o->ubuf)
	xfreeubuf(o->ubuf);
    o->ubuf = 0;
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
    RETURN_INT(eid);
}

PHP_FUNCTION(sp_close) {
    int rc;
    int eid = 0;

    rc = zend_parse_parameters(ZEND_NUM_ARGS(), "i", &eid);
    if (rc == FAILURE)
	return;
    rc = sp_close(eid);
    RETURN_INT(rc);
}

PHP_FUNCTION(sp_send) {
}

PHP_FUNCTION(sp_recv) {
}

PHP_FUNCTION(sp_listen) {
}

PHP_FUNCTION(sp_connect) {
}


static function_entry xio_functions[] = {
    PHP_ME(xmessage, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
    PHP_ME(xmessage, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
    PHP_FE(sp_close,     0)
    PHP_FE(sp_endpoint,  0)
    PHP_FE(sp_close,     0)
    PHP_FE(sp_send,      0)
    PHP_FE(sp_recv,      0)
    PHP_FE(sp_listen,    0)
    PHP_FE(sp_connect,   0)
    {0, 0, 0}
};

typedef struct {
    const char *name;
    int value;
} php_xio_constant;

static php_xio_constant xio_consts[] = {
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
    php_xio_constant *c;

    for (i = 0; i < NELEM(xio_consts, php_xio_constant); i++) {
	c = &xio_consts[i];
	REGISTER_LONG_CONSTANT(c->name, c->value, CONST_CS | CONST_PERSISTENT);
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

