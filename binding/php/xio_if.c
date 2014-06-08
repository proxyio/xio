/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <php.h>
#include <xio/socket.h>
#include <xio/poll.h>
#include <xio/sp.h>
#include <xio/sp_reqrep.h>
#include "../../src/utils/base.h"
#include "xio_if.h"

#define ZPARSE_ARGS(fmt, ...)						\
    zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, fmt, ##__VA_ARGS__)

PHP_FUNCTION(xallocubuf)
{
	int rc;
	char *str;
	int str_sz;
	char *ubuf;

	if ((rc = ZPARSE_ARGS("s", &str, &str_sz)) == FAILURE)
		return;
	ubuf = xallocubuf(str_sz);
	memcpy(ubuf, str, str_sz);
	RETURN_LONG((long)ubuf);
}

PHP_FUNCTION(xfreeubuf)
{
	int rc;
	char *ubuf = 0;

	if ((rc = ZPARSE_ARGS("l", &ubuf)) == FAILURE)
		return;
	xfreeubuf(ubuf);
	RETURN_NULL();
}

PHP_FUNCTION(xubuflen)
{
	int rc;
	char *ubuf = 0;

	if ((rc = ZPARSE_ARGS("l", &ubuf)) == FAILURE)
		return;
	RETURN_LONG(xubuflen(ubuf));
}


PHP_FUNCTION(xsocket)
{
	int rc;
	long pf = 0, socktype = 0;

	if ((rc = ZPARSE_ARGS("ll", &pf, &socktype)) == FAILURE)
		return;
	RETURN_LONG(xsocket((int)pf, (int)socktype));
}

PHP_FUNCTION(xbind)
{
	int rc;
	long fd = 0;
	const char *sockaddr = 0;
	int socklen = 0;

	if ((rc = ZPARSE_ARGS("ls", &fd, &sockaddr, &socklen)) == FAILURE)
		return;
	RETURN_LONG(xbind((int)fd, sockaddr));
}

PHP_FUNCTION(xaccept)
{
	int rc;
	long fd = 0;

	if ((rc = ZPARSE_ARGS("l", &fd)) == FAILURE)
		return;
	RETURN_LONG(xaccept((int)fd));
}


PHP_FUNCTION(xlisten)
{
	int rc;
	const char *sockaddr = 0;
	int socklen = 0;

	if ((rc = ZPARSE_ARGS("s", &sockaddr, &socklen)) == FAILURE)
		return;
	RETURN_LONG(xlisten(sockaddr));
}


PHP_FUNCTION(xconnect)
{
	int rc;
	const char *sockaddr = 0;
	int socklen = 0;

	if ((rc = ZPARSE_ARGS("s", &sockaddr, &socklen)) == FAILURE)
		return;
	RETURN_LONG(xconnect(sockaddr));
}


PHP_FUNCTION(xrecv)
{
	int rc;
	long fd = 0;
	char *ubuf = 0;

	if ((rc = ZPARSE_ARGS("l", &fd)) == FAILURE)
		return;
	xrecv((int)fd, &ubuf);
	RETURN_LONG((long)ubuf);
}


PHP_FUNCTION(xsend)
{
	int rc;
	long fd = 0;
	char *ubuf = 0;

	if ((rc = ZPARSE_ARGS("ll", &fd, &ubuf)) == FAILURE)
		return;
	RETURN_LONG(xsend((int)fd, ubuf));
}

PHP_FUNCTION(xclose)
{
	int rc;
	long fd = 0;

	if ((rc = ZPARSE_ARGS("l", &fd)) == FAILURE)
		return;
	RETURN_LONG(xclose((int)fd));
}

PHP_FUNCTION(xsetopt)
{
	RETURN_NULL();
}

PHP_FUNCTION(xgetopt)
{
	RETURN_NULL();
}

PHP_FUNCTION(sp_endpoint)
{
	int rc;
	int eid = 0;
	long sp_family = 0;
	long sp_type = 0;

	if ((rc = ZPARSE_ARGS("ll", &sp_family, &sp_type)) == FAILURE)
		return;
	eid = sp_endpoint((int)sp_family, (int)sp_type);
	RETURN_LONG(eid);
}

PHP_FUNCTION(sp_close)
{
	int rc;
	long eid = 0;

	if ((rc = ZPARSE_ARGS("l", &eid)) == FAILURE)
		return;
	rc = sp_close((int)eid);
	RETURN_LONG(rc);
}

PHP_FUNCTION(sp_send)
{
	int rc;
	long eid = 0;
	char *ubuf = 0;

	if ((rc = ZPARSE_ARGS("ll", &eid, &ubuf)) == FAILURE)
		return;
	RETURN_LONG(sp_send((int)eid, ubuf));
}

PHP_FUNCTION(sp_recv)
{
	int rc;
	long eid = 0;
	char *ubuf = 0;

	if ((rc = ZPARSE_ARGS("l", &eid)) == FAILURE)
		return;
	sp_recv((int)eid, &ubuf);
	RETURN_LONG((long)ubuf);
}

PHP_FUNCTION(sp_listen)
{
	int rc;
	long eid = 0;
	const char *sockaddr = 0;
	int socklen = 0;

	if ((rc = ZPARSE_ARGS("ls", &eid, &sockaddr, &socklen)) == FAILURE)
		return;
	RETURN_LONG(sp_listen((int)eid, sockaddr));
}

PHP_FUNCTION(sp_connect)
{
	int rc;
	long eid = 0;
	const char *sockaddr = 0;
	int socklen = 0;

	if ((rc = ZPARSE_ARGS("ls", &eid, &sockaddr, &socklen)) == FAILURE)
		return;
	RETURN_LONG(sp_connect((int)eid, sockaddr));
}

PHP_FUNCTION(sp_setopt)
{
	RETURN_NULL();
}

PHP_FUNCTION(sp_getopt)
{
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
	PHP_FE(sp_getopt,    0) {
		0, 0, 0
	}
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

// Patching for xio modules
#define XREGISTER_LONG_CONSTANT(name, lval, flags)			\
    zend_register_long_constant((name), strlen(name) + 1, (lval),	\
				(flags), module_number TSRMLS_CC)

PHP_MINIT_FUNCTION(xio)
{
	int i;
	struct xsymbol *sb;

	assert (sizeof(long) == sizeof(void *));

	for (i = 0; i < NELEM(const_symbols, struct xsymbol); i++) {
		sb = &const_symbols[i];
		DEBUG_OFF("register constant %s as %d", sb->name, sb->value);
		XREGISTER_LONG_CONSTANT(sb->name, sb->value, CONST_CS | CONST_PERSISTENT);
	}
}

PHP_MINFO_FUNCTION(xio)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "xio support", "enabled");
	php_info_print_table_end();
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
	PHP_MINFO(xio),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_XIO_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_XIO
ZEND_GET_MODULE(xio)
#endif

