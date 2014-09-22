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
#include "php_xio.h"

#define ZPARSE_ARGS(fmt, ...)						\
    zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, fmt, ##__VA_ARGS__)

static zend_class_entry *msg_ce = 0;
static zend_class_entry *msghdr_ce = 0;

ZEND_METHOD (Msghdr, __construct)
{
}

ZEND_METHOD (Msghdr, __destruct)
{
	int rc;
	zval **hdr = 0;
	char *ptr;

	if (zend_hash_find (Z_OBJPROP_P (getThis ()), "__ptr", sizeof("__ptr"), (void **) &hdr) == FAILURE)
		return;
	if ((ptr = Z_STRVAL_PP (hdr)) && (ptr = * (char **) ptr)) {
		ufree (ptr);
	}
}

static zend_function_entry msghdr_ce_method[] = {
	ZEND_ME (Msghdr, __construct,    NULL,   ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	ZEND_ME (Msghdr, __destruct,     NULL,   ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	{ NULL, NULL, NULL }
};

ZEND_METHOD (Msg, __construct2)
{
	zval *hdr = 0;

	ALLOC_ZVAL (hdr);
	object_init_ex (hdr, msghdr_ce);

	if (zend_hash_update (Z_OBJPROP_P (getThis ()), "hdr", sizeof ("hdr"), &hdr, sizeof (hdr), 0) == FAILURE)
		BUG_ON (1);
}

ZEND_METHOD (Msg, __construct)
{
}

ZEND_METHOD (Msg, __destruct)
{
}

static zend_function_entry msg_ce_method[] = {
	ZEND_ME (Msg, __construct,    NULL,   ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	ZEND_ME (Msg, __destruct,     NULL,   ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	{ NULL, NULL, NULL }
};


PHP_FUNCTION (sp_endpoint)
{
	int rc;
	int eid = 0;
	long sp_family = 0;
	long sp_type = 0;

	if ((rc = ZPARSE_ARGS ("ll", &sp_family, &sp_type)) == FAILURE)
		return;
	eid = sp_endpoint ((int) sp_family, (int) sp_type);
	RETURN_LONG (eid);
}

PHP_FUNCTION (sp_close)
{
	int rc;
	long eid = 0;

	if ((rc = ZPARSE_ARGS ("l", &eid)) == FAILURE)
		return;
	rc = sp_close ((int) eid);
	RETURN_LONG (rc);
}

PHP_FUNCTION (sp_send)
{
	int rc;
	long eid = 0;
	zval *msg = 0;
	zval **data = 0;
	zval **hdr = 0;
	zval **__ptr = 0;
	char *ptr;
	char *ubuf;

	if ((rc = ZPARSE_ARGS ("lO", &eid, &msg, msg_ce)) == FAILURE)
		return;
	if (zend_hash_find (Z_OBJPROP_P (msg), "data", sizeof ("data"), (void **) &data) == FAILURE)
		return;
	ubuf = ualloc (Z_STRLEN_PP (data));
	memcpy (ubuf, Z_STRVAL_PP (data), Z_STRLEN_PP (data));

	if (zend_hash_find (Z_OBJPROP_P (msg), "hdr", sizeof ("hdr"), (void **) &hdr) == SUCCESS && !ZVAL_IS_NULL (*hdr)) {
		if (zend_hash_find (Z_OBJPROP_PP (hdr), "__ptr", sizeof ("__ptr"), (void **) &__ptr) == SUCCESS) {
			if ((ptr = Z_STRVAL_PP (__ptr)) && (ptr = * (char **) ptr)) {
				uctl (ptr, SCOPY, ubuf);
			}
		}
	}
	if ((rc = sp_send ((int) eid, ubuf)))
		ufree (ubuf);
	RETURN_LONG (rc);
}

PHP_FUNCTION (sp_recv)
{
	int rc;
	long eid = 0;
	zval *msg = 0;
	zval *data = 0;
	zval *hdr = 0;
	zval *__ptr = 0;
	char *ubuf;

	if ((rc = ZPARSE_ARGS ("lO", &eid, &msg, msg_ce)) == FAILURE)
		return;
	if ((rc = sp_recv ((int) eid, &ubuf)))
		RETURN_LONG (rc);

	MAKE_STD_ZVAL (data);
	ZVAL_STRINGL (data, ubuf, usize (ubuf), 1);

	if (zend_hash_update (Z_OBJPROP_P (msg), "data", sizeof ("data"), &data, sizeof (data), 0) == FAILURE)
		BUG_ON (1);

	MAKE_STD_ZVAL (__ptr);
	ZVAL_STRINGL (__ptr, (char *) &ubuf, sizeof (ubuf) + 1, 1);
	ALLOC_ZVAL (hdr);
	object_init_ex (hdr, msghdr_ce);
	if (zend_hash_update (Z_OBJPROP_P (hdr), "__ptr", sizeof ("__ptr"), &__ptr, sizeof (__ptr), 0) == FAILURE)
		BUG_ON (1);

	if (zend_hash_update (Z_OBJPROP_P (msg), "hdr", sizeof ("hdr"), &hdr, sizeof (hdr), 0) == FAILURE)
		BUG_ON (1);
	RETURN_LONG (rc);
}

PHP_FUNCTION (sp_listen)
{
	int rc;
	long eid = 0;
	const char *sockaddr = 0;
	int socklen = 0;

	if ((rc = ZPARSE_ARGS ("ls", &eid, &sockaddr, &socklen)) == FAILURE)
		return;
	RETURN_LONG (sp_listen ((int) eid, sockaddr));
}

PHP_FUNCTION (sp_connect)
{
	int rc;
	long eid = 0;
	const char *sockaddr = 0;
	int socklen = 0;

	if ((rc = ZPARSE_ARGS ("ls", &eid, &sockaddr, &socklen)) == FAILURE)
		return;
	RETURN_LONG (sp_connect ((int) eid, sockaddr));
}

PHP_FUNCTION (sp_setopt)
{
	RETURN_NULL();
}

PHP_FUNCTION (sp_getopt)
{
	RETURN_NULL();
}

static function_entry xio_functions[] = {
	PHP_FE (sp_endpoint,  0)
	PHP_FE (sp_close,     0)
	PHP_FE (sp_send,      0)
	PHP_FE (sp_recv,      0)
	PHP_FE (sp_listen,    0)
	PHP_FE (sp_connect,   0)
	PHP_FE (sp_setopt,    0)
	PHP_FE (sp_getopt,    0)
	{ 0, 0, 0 }
};

struct sym_kv {
	const char name[32];
	int value;
};

static struct sym_kv const_symbols[] = {
	{"SP_REQREP",    SP_REQREP},
	{"SP_BUS",       SP_BUS},
	{"SP_PUBSUB",    SP_PUBSUB},

	{"SP_REQ",       SP_REQ},
	{"SP_REP",       SP_REP},
	{"SP_PROXY",     SP_PROXY},
};


/* Patching for xio modules */
#define XREGISTER_LONG_CONSTANT(name, lval, flags)			\
    zend_register_long_constant((name), strlen(name) + 1, (lval),	\
				(flags), module_number TSRMLS_CC)

PHP_MINIT_FUNCTION (xio)
{
	int i;
	struct sym_kv *sb;
	zend_class_entry msg_ce_ptr;
	zend_class_entry msghdr_ce_ptr;
	
	INIT_CLASS_ENTRY (msg_ce_ptr, "Msg", msg_ce_method);
	msg_ce = zend_register_internal_class (&msg_ce_ptr TSRMLS_CC);
	zend_declare_property_null (msg_ce, "data", strlen("data"), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null (msg_ce, "hdr", strlen("hdr"), ZEND_ACC_PUBLIC TSRMLS_CC);

	INIT_CLASS_ENTRY (msghdr_ce_ptr, "Msghdr", msghdr_ce_method);
	msghdr_ce = zend_register_internal_class (&msghdr_ce_ptr TSRMLS_CC);
	zend_declare_property_null (msghdr_ce, "__ptr", strlen("__ptr"), ZEND_ACC_PUBLIC TSRMLS_CC);
	
	BUG_ON (sizeof (long) != sizeof (void *));

	for (i = 0; i < NELEM (const_symbols, struct sym_kv); i++) {
		sb = &const_symbols[i];
		DEBUG_OFF ("register constant %s as %d", sb->name, sb->value);
		XREGISTER_LONG_CONSTANT (sb->name, sb->value, CONST_CS | CONST_PERSISTENT);
	}

	return 0;
}

PHP_MINFO_FUNCTION (xio)
{
	php_info_print_table_start();
	php_info_print_table_header (2, "xio support", "enabled");
	php_info_print_table_end();
}

zend_module_entry xio_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	PHP_XIO_EXTNAME,
	xio_functions,
	PHP_MINIT (xio),
	NULL,
	NULL,
	NULL,
	PHP_MINFO (xio),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_XIO_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_XIO
ZEND_GET_MODULE (xio)
#endif

