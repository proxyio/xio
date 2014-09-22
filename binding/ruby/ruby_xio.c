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

#include <ruby.h>
#include <xio/socket.h>
#include <xio/poll.h>
#include <xio/sp.h>
#include <xio/sp_reqrep.h>
#include "../../src/utils/base.h"


static VALUE rb_sp_endpoint (VALUE self, VALUE sp_family, VALUE sp_type)
{
	int _sp_family = FIX2INT (sp_family);
	int _sp_type = FIX2INT (sp_type);

	return INT2FIX (sp_endpoint (_sp_family, _sp_type));
}


static VALUE rb_sp_close (VALUE self, VALUE eid)
{
	int _eid = FIX2INT (eid);

	return INT2FIX (sp_close (_eid));
}


static VALUE rb_sp_send (VALUE self, VALUE eid, VALUE msg)
{
	int _eid = FIX2INT (eid);
	int rc;
	VALUE rb_hdr = 0;
	char *hdr = 0;
	char *ubuf = ualloc (RSTRING (msg)->len);

	memcpy (ubuf, RSTRING (msg)->ptr, usize (ubuf));

	/* How can i checking the valid rb_values, fucking the ruby extension here
	 */
	if ((rb_hdr = rb_iv_get (msg, "@hdr")) != 4) {
		Data_Get_Struct (rb_hdr, char, hdr);
		if (hdr)
			uctl (hdr, SCOPY, ubuf);
	}
	if ((rc = sp_send (_eid, ubuf)))
		ufree (ubuf);
	return INT2FIX (rc);
}

static VALUE rb_sp_recv (VALUE self, VALUE eid)
{
	int _eid = FIX2INT (eid);
	int rc;
	char *ubuf = 0;
	VALUE msg;

	if ((rc = sp_recv (_eid, &ubuf)))
		return Qnil;
	msg = rb_str_new (ubuf, usize (ubuf));
	rb_iv_set (msg, "@hdr", Data_Wrap_Struct (0, 0, ufree, ubuf));
	return msg;
}

static VALUE rb_sp_add (VALUE self, VALUE eid, VALUE fd)
{
	int _eid = FIX2INT (eid);
	int _fd = FIX2INT (fd);

	return INT2FIX (sp_add (_eid, _fd));
}

static VALUE rb_sp_rm (VALUE self, VALUE eid, VALUE fd)
{
	int _eid = FIX2INT (eid);
	int _fd = FIX2INT (fd);

	return INT2FIX (sp_rm (_eid, _fd));
}


static VALUE rb_sp_setopt (VALUE self, VALUE eid)
{
	return Qnil;
}

static VALUE rb_sp_getopt (VALUE self, VALUE eid)
{
	return Qnil;
}

static VALUE rb_sp_listen (VALUE self, VALUE eid, VALUE sockaddr)
{
	int _eid = FIX2INT (eid);
	const char *_sockaddr = RSTRING (sockaddr)->ptr;

	return INT2FIX (sp_listen (_eid, _sockaddr));
}

static VALUE rb_sp_connect (VALUE self, VALUE eid, VALUE sockaddr)
{
	int _eid = FIX2INT (eid);
	const char *_sockaddr = RSTRING (sockaddr)->ptr;

	return INT2FIX (sp_connect (_eid, _sockaddr));
}


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


/* Prototype for the initialization method - Ruby calls this, not you
 */
void Init_xio()
{
	int i;
	struct sym_kv *sym;

	for (i = 0; i < NELEM (const_symbols, struct sym_kv); i++) {
		sym = &const_symbols[i];
		rb_define_global_const (sym->name, INT2FIX (sym->value));
	}

	rb_define_global_function ("sp_endpoint",     rb_sp_endpoint,    2);
	rb_define_global_function ("sp_close",        rb_sp_close,       1);
	rb_define_global_function ("sp_send",         rb_sp_send,        2);
	rb_define_global_function ("sp_recv",         rb_sp_recv,        1);
	rb_define_global_function ("sp_add",          rb_sp_add,         2);
	rb_define_global_function ("sp_rm",           rb_sp_rm,          2);
	rb_define_global_function ("sp_setopt",       rb_sp_setopt,      1);
	rb_define_global_function ("sp_getopt",       rb_sp_getopt,      1);
	rb_define_global_function ("sp_listen",       rb_sp_listen,      2);
	rb_define_global_function ("sp_connect",      rb_sp_connect,     2);
}
