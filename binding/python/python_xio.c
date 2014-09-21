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

#include <utils/base.h>
#include <xio/socket.h>
#include <xio/poll.h>
#include <xio/sp.h>
#include <xio/sp_reqrep.h>
#include <string.h>
#include <python2.6/Python.h>
#include <python2.6/structmember.h>

#include "msgobject.c"

static PyObject *cpy_sp_endpoint (PyObject *self, PyObject *args)
{
	int sp_family = 0;
	int sp_type = 0;

	if (!PyArg_ParseTuple (args, "ii", &sp_family, &sp_type))
		return 0;
	return Py_BuildValue ("i", sp_endpoint (sp_family, sp_type));
}

static PyObject *cpy_sp_close (PyObject *self, PyObject *args)
{
	int eid = 0;

	if (!PyArg_ParseTuple (args, "i", &eid))
		return 0;
	return Py_BuildValue ("i", sp_close (eid));
}

static PyObject *cpy_sp_send (PyObject *self, PyObject *args)
{
	int rc;
	int eid = 0;
	Msg *msg = 0;
	Msghdr *hdr = 0;
	char *ubuf = 0;
	i64 leng = 0;
	char *buff = 0;

	if (!PyArg_ParseTuple (args, "iO", &eid, &msg))
		return 0;
	if ((rc = PyString_AsStringAndSize (msg->data, &buff, &leng)))
		return 0;
	ubuf = ualloc (leng);
	memcpy (ubuf, buff, leng);
	if (PyObject_TypeCheck (msg->hdr, &Msghdr_Type)) {
		hdr = (Msghdr *) msg->hdr;
		uctl (hdr->ubuf, SCOPY, ubuf);
	}
	if ((rc = sp_send (eid, ubuf)))
		ufree (ubuf);
	return Py_BuildValue ("i", rc);
}

static PyObject *cpy_sp_recv (PyObject *self, PyObject *args)
{
	Msg *msg;
	Msghdr *hdr;
	int eid = 0;
	int rc = 0;
	char *ubuf = 0;

	if (!PyArg_ParseTuple (args, "i", &eid))
		return 0;
	if ((rc = sp_recv (eid, &ubuf)) != 0)
		return Py_BuildValue ("is", rc, "error");
	msg = (Msg *) PyType_GenericAlloc (&Msg_Type, 0);
	msg->hdr = PyType_GenericAlloc (&Msghdr_Type, 0);
	hdr = (Msghdr *) msg->hdr;
	hdr->ubuf = ubuf;
	msg->data = PyString_FromStringAndSize (ubuf, ulength (ubuf));
	return Py_BuildValue ("iN", rc, msg);
}

static PyObject *cpy_sp_add (PyObject *self, PyObject *args)
{
	int eid = 0;
	int sockfd = 0;

	if (!PyArg_ParseTuple (args, "ii", &eid, &sockfd))
		return 0;
	return Py_BuildValue ("i", sp_add (eid, sockfd));
}

static PyObject *cpy_sp_rm (PyObject *self, PyObject *args)
{
	int eid = 0;
	int sockfd = 0;

	if (!PyArg_ParseTuple (args, "ii", &eid, &sockfd))
		return 0;
	return Py_BuildValue ("i", sp_rm (eid, sockfd));
}

static PyObject *cpy_sp_setopt (PyObject *self, PyObject *args)
{
	return 0;
}


static PyObject *cpy_sp_getopt (PyObject *self, PyObject *args)
{
	return 0;
}

static PyObject *cpy_sp_listen (PyObject *self, PyObject *args)
{
	int eid = 0;
	const char *sockaddr = 0;

	if (!PyArg_ParseTuple (args, "is", &eid, &sockaddr))
		return 0;
	return Py_BuildValue ("i", sp_listen (eid, sockaddr));
}


static PyObject *cpy_sp_connect (PyObject *self, PyObject *args)
{
	int eid = 0;
	const char *sockaddr = 0;

	if (!PyArg_ParseTuple (args, "is", &eid, &sockaddr))
		return 0;
	return Py_BuildValue ("i", sp_connect (eid, sockaddr));
}

static PyMethodDef module_methods[] = {
	{"sp_endpoint",     cpy_sp_endpoint,    METH_VARARGS,  "create an SP endpoint"},
	{"sp_close",        cpy_sp_close,       METH_VARARGS,  "close an SP endpoint"},
	{"sp_send",         cpy_sp_send,        METH_VARARGS,  "send one message into endpoint"},
	{"sp_recv",         cpy_sp_recv,        METH_VARARGS,  "recv one message from endpoint"},
	{"sp_add",          cpy_sp_add,         METH_VARARGS,  "add socket to endpoint"},
	{"sp_rm",           cpy_sp_rm,          METH_VARARGS,  "remove socket from endpoint"},
	{"sp_setopt",       cpy_sp_setopt,      METH_VARARGS,  "set SP options"},
	{"sp_getopt",       cpy_sp_getopt,      METH_VARARGS,  "get SP options"},
	{"sp_listen",       cpy_sp_listen,      METH_VARARGS,  "helper API for add a listener to endpoint"},
	{"sp_connect",      cpy_sp_connect,     METH_VARARGS,  "helper API for add a connector to endpoint"},
	{NULL, NULL, 0, NULL}
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

PyMODINIT_FUNC initxio()
{
	PyObject *pyxio = Py_InitModule ("xio", module_methods);
	int i;
	struct sym_kv *sb;

	BUG_ON (PyType_Ready (&Msg_Type) < 0);
	BUG_ON (PyType_Ready (&Msghdr_Type) < 0);
	Py_INCREF (&Msg_Type);
	Py_INCREF (&Msghdr_Type);
	PyModule_AddObject (pyxio, "Msg", (PyObject *) &Msg_Type);
	PyModule_AddObject (pyxio, "Msghdr", (PyObject *) &Msghdr_Type);

	for (i = 0; i < NELEM (const_symbols, struct sym_kv); i++) {
		sb = &const_symbols[i];
		PyModule_AddIntConstant (pyxio, sb->name, sb->value);
	}
}
