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

typedef struct {
	PyObject_HEAD
	void *ubuf;
} Message;

static PyObject *Message_new_with_string (char *buff, int len);
static PyObject *Message_new (PyTypeObject *type, PyObject *args, PyObject *kwds);
static void Message_dealloc (PyObject *args);
static PyObject *Message_response (PyObject *self, PyObject *args);

static PyMemberDef Message_members[] = {
	{NULL}
};

static PyMethodDef Message_methods[] = {
	{"Response", Message_response, METH_VARARGS, "Generate salability protocols response"},
	{NULL, NULL, 0, NULL}        /* Sentinel */
};

static int Message_getreadbuffer (Message *self, int segment, void **ptrptr)
{
	if (segment != 0 || self->ubuf == NULL) {
		PyErr_BadInternalCall();
		return -1;
	}
	*ptrptr = ( (Message *) self)->ubuf;
	return xubuflen ( ( (Message *) self)->ubuf);
}

static int Message_getwritebuffer (Message *self, int segment, void **ptrptr)
{
	if (segment != 0 || self->ubuf == NULL) {
		PyErr_BadInternalCall();
		return -1;
	}
	*ptrptr = ( (Message *) self)->ubuf;
	return xubuflen ( ( (Message *) self)->ubuf);
}

static int Message_getsegcountproc (PyObject *self, int *lenp)
{
	if (lenp != NULL)
		*lenp = xubuflen ( ( (Message *) self)->ubuf);
	return 1;
}

static PyBufferProcs Message_bufferproces = {
	(readbufferproc)     Message_getreadbuffer,
	(writebufferproc)    Message_getwritebuffer,
	(segcountproc)       Message_getsegcountproc,
	NULL
};

static PyObject *Message_repr (Message *self);
static PyObject *Message_str (Message * self);

static PyTypeObject Message_Type = {
	PyVarObject_HEAD_INIT (NULL, 0)
	"_xio_cpy.Message",          /*tp_name*/
	sizeof (Message),            /*tp_basicsize*/
	0,                           /*tp_itemsize*/
	Message_dealloc,             /*tp_dealloc*/
	0,                           /*tp_print*/
	0,                           /*tp_getattr*/
	0,                           /*tp_setattr*/
	0,                           /*tp_compare*/
	(reprfunc) Message_repr,     /*tp_repr*/
	0,                           /*tp_as_number*/
	0,                           /*tp_as_sequence*/
	0,                           /*tp_as_mapping*/
	0,                           /*tp_hash */
	0,                           /*tp_call*/
	(reprfunc) Message_str,      /*tp_str*/
	0,                           /*tp_getattro*/
	0,                           /*tp_setattro*/
	&Message_bufferproces,       /*tp_as_buffer*/
	Py_TPFLAGS_HAVE_CLASS
	| Py_TPFLAGS_HAVE_NEWBUFFER,
	"allocated message wrapper supporting buffer protocol",
	/* tp_doc */
	0,                           /* tp_traverse */
	0,                           /* tp_clear */
	0,                           /* tp_richcompare */
	0,                           /* tp_weaklistoffset */
	0,                           /* tp_iter */
	0,                           /* tp_iternext */
	Message_methods,             /* tp_methods */
	Message_members,             /* tp_members */
	0,                           /* tp_getset */
	0,                           /* tp_base */
	0,                           /* tp_dict */
	0,                           /* tp_descr_get */
	0,                           /* tp_descr_set */
	0,                           /* tp_dictoffset */
	0,                           /* tp_init */
	0,                           /* tp_alloc */
	Message_new,                 /* tp_new */
};


static PyObject *Message_new_by_ubuf (char *ubuf)
{
	Message *msg = (Message *) PyType_GenericAlloc (&Message_Type, 0);
	msg->ubuf = ubuf;
	return (PyObject *) msg;
}

static PyObject *Message_new_with_string (char *buff, int len)
{
	Message *msg = (Message *) PyType_GenericAlloc (&Message_Type, 0);

	if (!(msg->ubuf = xallocubuf (len))) {
		Py_DECREF ((PyObject *)msg);
		Py_RETURN_NONE;
	}
	memcpy (msg->ubuf, buff, len);
	return (PyObject *) msg;
}

static PyObject *Message_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	char *buff = 0;

	if (!PyArg_ParseTuple (args, "s", &buff) )
		return 0;
	return Message_new_with_string (buff, strlen (buff));
}

static void Message_dealloc (PyObject *args)
{
	Message *message = (Message *)args;

	if ( message->ubuf )
		xfreeubuf (message->ubuf);
}

static PyObject *Message_response (PyObject *self, PyObject *args)
{
	Message *req = (Message *)self;
	Message *resp = (Message *) Message_new (&Message_Type, args, 0);

	BUG_ON (ubufctl (req->ubuf, SCOPY, resp->ubuf));
	return (PyObject *)resp;
}


static PyObject *Message_repr (Message *self)
{
	return PyUnicode_FromFormat ("<_xio_cpy.Message size %zu, address %p >",
	                             xubuflen (self->ubuf), self->ubuf);
}

static PyObject *Message_str (Message * self)
{
	return PyBytes_FromStringAndSize (self->ubuf, xubuflen (self->ubuf) );
}


static PyObject *cpy_sp_endpoint (PyObject *self, PyObject *args)
{
	int sp_family = 0;
	int sp_type = 0;

	if (!PyArg_ParseTuple (args, "ii", &sp_family, &sp_type) )
		return 0;
	return Py_BuildValue ("i", sp_endpoint (sp_family, sp_type) );
}

static PyObject *cpy_sp_close (PyObject *self, PyObject *args)
{
	int eid = 0;

	if (!PyArg_ParseTuple (args, "i", &eid) )
		return 0;
	return Py_BuildValue ("i", sp_close (eid) );
}

static PyObject *cpy_sp_send_with_string (int eid, PyObject *args)
{
	int rc;
	char *str = PyString_AsString (args);
	int length = PyString_Size (args);
	char *ubuf = xallocubuf (length);

	BUG_ON (!ubuf);
	memcpy (ubuf, str, length);
	if ((rc = sp_send (eid, ubuf)) != 0)
		xfreeubuf (ubuf);
	return Py_BuildValue ("i", rc);
}

static PyObject *cpy_sp_send_with_message (int eid, PyObject *args)
{
	int rc = -1;
	Message *msg = (Message *)args;

	if (msg->ubuf && (rc = sp_send (eid, msg->ubuf)) == 0)
		msg->ubuf = 0;
	return Py_BuildValue ("i", rc);
}


static PyObject *cpy_sp_send (PyObject *self, PyObject *args)
{
	int eid = 0;
	PyObject *msg = 0;

	if (!PyArg_ParseTuple (args, "iO", &eid, &msg) )
		return 0;
	if (PyObject_TypeCheck (msg, &Message_Type))
		return cpy_sp_send_with_message (eid, msg);
	else if (PyObject_TypeCheck (msg, &PyString_Type))
		return cpy_sp_send_with_string (eid, msg);
	return 0;
}

static PyObject *cpy_sp_recv (PyObject *self, PyObject *args)
{
	PyObject *msg;
	int eid = 0;
	int rc = 0;
	char *ubuf = 0;

	if (!PyArg_ParseTuple (args, "i", &eid) )
		return 0;
	if ((rc = sp_recv (eid, &ubuf)) != 0)
		return Py_BuildValue ("is", rc, "error");
	msg = Message_new_by_ubuf (ubuf);
	return Py_BuildValue ("iO", rc, msg);
}

static PyObject *cpy_sp_add (PyObject *self, PyObject *args)
{
	int eid = 0;
	int sockfd = 0;

	if (!PyArg_ParseTuple (args, "ii", &eid, &sockfd) )
		return 0;
	return Py_BuildValue ("i", sp_add (eid, sockfd) );
}

static PyObject *cpy_sp_rm (PyObject *self, PyObject *args)
{
	int eid = 0;
	int sockfd = 0;

	if (!PyArg_ParseTuple (args, "ii", &eid, &sockfd) )
		return 0;
	return Py_BuildValue ("i", sp_rm (eid, sockfd) );
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

	if (!PyArg_ParseTuple (args, "is", &eid, &sockaddr) )
		return 0;
	return Py_BuildValue ("i", sp_listen (eid, sockaddr) );
}


static PyObject *cpy_sp_connect (PyObject *self, PyObject *args)
{
	int eid = 0;
	const char *sockaddr = 0;

	if (!PyArg_ParseTuple (args, "is", &eid, &sockaddr) )
		return 0;
	return Py_BuildValue ("i", sp_connect (eid, sockaddr) );
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

struct xsymbol {
	const char name[32];
	int value;
};

static struct xsymbol const_symbols[] = {
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
	struct xsymbol *sb;

	BUG_ON (PyType_Ready (&Message_Type) < 0);
	Py_INCREF (&Message_Type);
	PyModule_AddObject (pyxio, "Message", (PyObject *) &Message_Type);
	for (i = 0; i < NELEM (const_symbols, struct xsymbol); i++) {
		sb = &const_symbols[i];
		PyModule_AddIntConstant (pyxio, sb->name, sb->value);
	}
}
