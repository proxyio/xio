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

#include <xio/socket.h>
#include <xio/poll.h>
#include <xio/sp.h>
#include <xio/sp_reqrep.h>
#include "../../src/utils/base.h"
#include "ruby.h"

/* Defining a space for information and references about the module to be
 * stored internally
 */


static VALUE rb_xallocubuf(VALUE self, VALUE rb_str) {
    char *ubuf = xallocubuf(RSTRING(rb_str)->len);
    memcpy(ubuf, RSTRING(rb_str)->ptr, xubuflen(ubuf));
    return INT2NUM((long)ubuf);
}

static VALUE rb_xubuflen(VALUE self, VALUE rb_ubuf) {
    char *ubuf = (char *)NUM2LONG(rb_ubuf);;
    return INT2NUM(xubuflen(ubuf));
}

static VALUE rb_xfreeubuf(VALUE self, VALUE rb_ubuf) {
    char *ubuf = (char *)NUM2LONG(rb_ubuf);
    xfreeubuf(ubuf);
    return Qnil;
}

static VALUE rb_xsocket(VALUE self, VALUE pf, VALUE socktype) {
    int _pf = FIX2INT(pf);
    int _socktype = FIX2INT(socktype);

    return INT2FIX(xsocket(_pf, _socktype));
}

static VALUE rb_xbind(VALUE self, VALUE fd, VALUE sockaddr) {
    int _fd = FIX2INT(fd);
    const char *_sockaddr = RSTRING(sockaddr)->ptr;

    return INT2FIX(xbind(_fd, _sockaddr));
}

static VALUE rb_xaccept(VALUE self, VALUE fd) {
    int _fd = FIX2INT(fd);

    return INT2FIX(xaccept(_fd));
}

static VALUE rb_xlisten(VALUE self, VALUE sockaddr) {
    const char *_sockaddr = RSTRING(sockaddr)->ptr;

    return INT2FIX(xlisten(_sockaddr));
}

static VALUE rb_xconnect(VALUE self, VALUE sockaddr) {
    const char *_sockaddr = RSTRING(sockaddr)->ptr;

    return INT2FIX(xconnect(_sockaddr));
}

static VALUE rb_xrecv(VALUE self, VALUE fd) {
    int _fd = FIX2INT(fd);
    int rc;
    char *ubuf = 0;

    rc = xrecv(_fd, &ubuf);
    if (!rc)
	assert (ubuf != 0);
    return INT2NUM((long)ubuf);
}

static VALUE rb_xsend(VALUE self, VALUE fd, VALUE ubuf) {
    int _fd = FIX2INT(fd);
    char *_ubuf = (char *)NUM2LONG(ubuf);

    return INT2FIX(xsend(_fd, _ubuf));
}

static VALUE rb_xclose(VALUE self, VALUE fd) {
    int _fd = FIX2INT(fd);

    return INT2FIX(xclose(_fd));
}


static VALUE rb_xgetopt(VALUE self, VALUE fd) {
    return Qnil;
}

static VALUE rb_xsetopt(VALUE self, VALUE fd) {
    return Qnil;
}

static VALUE rb_sp_endpoint(VALUE self, VALUE sp_family, VALUE sp_type) {
    int _sp_family = FIX2INT(sp_family);
    int _sp_type = FIX2INT(sp_type);

    return INT2FIX(sp_endpoint(_sp_family, _sp_type));
}


static VALUE rb_sp_close(VALUE self, VALUE eid) {
    int _eid = FIX2INT(eid);

    return INT2FIX(sp_close(_eid));
}


static VALUE rb_sp_send(VALUE self, VALUE eid, VALUE ubuf) {
    int _eid = FIX2INT(eid);
    int rc;
    char *_ubuf = (char *)NUM2LONG(ubuf);

    rc = sp_send(_eid, _ubuf);
    return INT2FIX(rc);
}

static VALUE rb_sp_recv(VALUE self, VALUE eid) {
    int _eid = FIX2INT(eid);
    int rc;
    char *ubuf = 0;

    rc = sp_recv(_eid, &ubuf);
    if (!rc)
	assert (ubuf != 0);
    return INT2NUM((long)ubuf);
}

static VALUE rb_sp_add(VALUE self, VALUE eid, VALUE fd) {
    int _eid = FIX2INT(eid);
    int _fd = FIX2INT(fd);

    return INT2FIX(sp_add(_eid, _fd));
}

static VALUE rb_sp_rm(VALUE self, VALUE eid, VALUE fd) {
    int _eid = FIX2INT(eid);
    int _fd = FIX2INT(fd);

    return INT2FIX(sp_rm(_eid, _fd));
}


static VALUE rb_sp_setopt(VALUE self, VALUE eid) {
    return Qnil;
}

static VALUE rb_sp_getopt(VALUE self, VALUE eid) {
    return Qnil;
}

static VALUE rb_sp_listen(VALUE self, VALUE eid, VALUE sockaddr) {
    int _eid = FIX2INT(eid);
    const char *_sockaddr = RSTRING(sockaddr)->ptr;

    return INT2FIX(sp_listen(_eid, _sockaddr));
}

static VALUE rb_sp_connect(VALUE self, VALUE eid, VALUE sockaddr) {
    int _eid = FIX2INT(eid);
    const char *_sockaddr = RSTRING(sockaddr)->ptr;

    return INT2FIX(sp_connect(_eid, _sockaddr));
}


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


// Prototype for the initialization method - Ruby calls this, not you
void Init_xio() {
    int i;
    struct xsymbol *sb;

    for (i = 0; i < NELEM(const_symbols, struct xsymbol); i++) {
	sb = &const_symbols[i];
	rb_define_global_const(sb->name, INT2FIX(sb->value));
    }

    rb_define_global_function("xallocubuf",      rb_xallocubuf,     1);
    rb_define_global_function("xubuflen",        rb_xubuflen,       1);
    rb_define_global_function("xfreeubuf",       rb_xfreeubuf,      1);
    rb_define_global_function("xsocket",         rb_xsocket,        2);
    rb_define_global_function("xbind",           rb_xbind,          2);
    rb_define_global_function("xaccept",         rb_xaccept,        1);
    rb_define_global_function("xlisten",         rb_xlisten,        1);
    rb_define_global_function("xconnect",        rb_xconnect,       1);
    rb_define_global_function("xrecv",           rb_xrecv,          1);
    rb_define_global_function("xsend",           rb_xsend,          2);
    rb_define_global_function("xclose",          rb_xclose,         1);
    rb_define_global_function("xgetopt",         rb_xgetopt,        1);
    rb_define_global_function("xsetopt",         rb_xsetopt,        1);

    rb_define_global_function("sp_endpoint",     rb_sp_endpoint,    2);
    rb_define_global_function("sp_close",        rb_sp_close,       1);
    rb_define_global_function("sp_send",         rb_sp_send,        2);
    rb_define_global_function("sp_recv",         rb_sp_recv,        1);
    rb_define_global_function("sp_add",          rb_sp_add,         2);
    rb_define_global_function("sp_rm",           rb_sp_rm,          2);
    rb_define_global_function("sp_setopt",       rb_sp_setopt,      1);
    rb_define_global_function("sp_getopt",       rb_sp_getopt,      1);
    rb_define_global_function("sp_listen",       rb_sp_listen,      2);
    rb_define_global_function("sp_connect",      rb_sp_connect,     2);
}
