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
#include "py_xio.h"

const static char MODULE_NAME[] = "_xio_cpy";

typedef struct {
    PyObject_HEAD
    void *ubuf;
} Message;

static PyObject *Message_new(PyTypeObject *type, PyObject *args,
			     PyObject *kwds) {
    PyErr_Format(PyExc_TypeError,
                 "cannot create '%.100s' instances us xallocubuf instead",
                 type->tp_name);
    return 0;
}

static PyMemberDef Message_members[] = {
    {NULL}
};

static PyMethodDef Message_methods[] = {
    {NULL}
};


static int Message_getreadbuffer(Message *self, int segment, void **ptrptr) {
    if (segment != 0 || self->ubuf == NULL) {
	PyErr_BadInternalCall();
	return -1;
    }
    *ptrptr = ((Message *)self)->ubuf;
    return xubuflen(((Message *)self)->ubuf);
}

static int Message_getwritebuffer(Message *self, int segment, void **ptrptr) {
    if (segment != 0 || self->ubuf == NULL) {
        PyErr_BadInternalCall();
        return -1;
    }
    *ptrptr = ((Message *)self)->ubuf;
    return xubuflen(((Message *)self)->ubuf);
}

static int Message_getsegcountproc(PyObject *self, int *lenp) {
    if (lenp != NULL)
        *lenp = xubuflen(((Message *)self)->ubuf);
    return 1;
}

static PyBufferProcs Message_bufferproces = {
    (readbufferproc)     Message_getreadbuffer,
    (writebufferproc)    Message_getwritebuffer,
    (segcountproc)       Message_getsegcountproc,
    NULL
};

static PyObject *Message_repr(Message *self) {
    return PyUnicode_FromFormat("<_xio_cpy.Message size %zu, address %p >",
				xubuflen(self->ubuf), self->ubuf);
}

static PyObject *Message_str(Message * self) {
    return PyBytes_FromStringAndSize(self->ubuf, xubuflen(self->ubuf));
}

static PyTypeObject MessageType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_xio_cpy.Message",          /*tp_name*/
    sizeof(Message),             /*tp_basicsize*/
    0,                           /*tp_itemsize*/
    0,                           /*tp_dealloc*/
    0,                           /*tp_print*/
    0,                           /*tp_getattr*/
    0,                           /*tp_setattr*/
    0,                           /*tp_compare*/
    (reprfunc)Message_repr,      /*tp_repr*/
    0,                           /*tp_as_number*/
    0,                           /*tp_as_sequence*/
    0,                           /*tp_as_mapping*/
    0,                           /*tp_hash */
    0,                           /*tp_call*/
    (reprfunc)Message_str,       /*tp_str*/
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

static PyObject *cpy_xallocubuf(PyObject *self, PyObject *args) {
    int size = 0;
    Message *message = 0;

    if (!PyArg_ParseTuple(args, "i", &size))
        return 0;
    message = (Message *)PyType_GenericAlloc(&MessageType, 0);
    if ((message->ubuf = xallocubuf(size)) == NULL) {
        Py_DECREF((PyObject*)message);
        Py_RETURN_NONE;
    }
    return (PyObject*)message;
}

static PyObject *cpy_xfreeubuf(PyObject *self, PyObject *args) {
    Message *message = 0;

    if (!PyArg_ParseTuple(args, "O", &message))
	return 0;
    xfreeubuf(message->ubuf);
    Py_RETURN_NONE;
}


static PyObject *cpy_xubuflen (PyObject *self, PyObject *args) {
    Message *message = (Message *)self;
    return Py_BuildValue("i", xubuflen(message->ubuf));
}

static PyObject *cpy_xsocket(PyObject *self, PyObject *args) {
    int pf = 0;
    int socktype = 0;

    if (!PyArg_ParseTuple(args, "ii", &pf, &socktype))
        return 0;
    return Py_BuildValue("i", xsocket(pf, socktype));
}

static PyObject *cpy_xbind(PyObject *self, PyObject *args) {
    int fd = 0;
    const char *sockaddr = 0;

    if (!PyArg_ParseTuple(args, "is", &fd, &sockaddr))
        return 0;
    return Py_BuildValue("i", xbind(fd, sockaddr));
}

static PyObject *cpy_xaccept(PyObject *self, PyObject *args) {
    int fd = 0;

    if (!PyArg_ParseTuple(args, "i", &fd))
        return 0;
    return Py_BuildValue("i", xaccept(fd));
}

static PyObject *cpy_xlisten(PyObject *self, PyObject *args) {
    const char *sockaddr = 0;

    if (!PyArg_ParseTuple(args, "s", &sockaddr))
        return 0;
    return Py_BuildValue("i", xlisten(sockaddr));
}

static PyObject *cpy_xconnect(PyObject *self, PyObject *args) {
    const char *sockaddr = 0;

    if (!PyArg_ParseTuple(args, "s", &sockaddr))
        return 0;
    return Py_BuildValue("i", xconnect(sockaddr));
}

static PyObject *cpy_xrecv(PyObject *self, PyObject *args) {
    PyObject *tuple = PyTuple_New(2);
    Message *message = (Message *)PyType_GenericAlloc(&MessageType, 0);
    int fd = 0;
    int rc;
    char *ubuf = 0;

    if (!PyArg_ParseTuple(args, "i", &fd))
	return 0;
    if ((rc = xrecv(fd, &ubuf)) == 0)
	message->ubuf = ubuf;
    PyTuple_SetItem(tuple, 0, Py_BuildValue("i", rc));
    PyTuple_SetItem(tuple, 1, (PyObject *)message);
    return tuple;
}

static PyObject *cpy_xsend(PyObject *self, PyObject *args) {
    int fd = 0;
    int rc;
    Message *message = 0;

    if (!PyArg_ParseTuple(args, "iO", &fd, &message))
	return 0;
    if (!message->ubuf) {
	PyErr_BadInternalCall();
	return 0;
    }
    if ((rc = xsend(fd, message->ubuf)) == 0) {
	message->ubuf = 0;
    }
    return Py_BuildValue("i", rc);
}

static PyObject *cpy_xclose(PyObject *self, PyObject *args) {
    int fd = 0;

    if (!PyArg_ParseTuple(args, "i", &fd))
        return 0;
    return Py_BuildValue("i", xclose(fd));
}

static PyObject *cpy_xsetopt(PyObject *self, PyObject *args) {
    return 0;
}

static PyObject *cpy_xgetopt(PyObject *self, PyObject *args) {
    return 0;
}

static PyObject *cpy_sp_endpoint(PyObject *self, PyObject *args) {
    int sp_family = 0;
    int sp_type = 0;

    if (!PyArg_ParseTuple(args, "ii", &sp_family, &sp_type))
        return 0;
    return Py_BuildValue("i", sp_endpoint(sp_family, sp_type));
}

static PyObject *cpy_sp_close(PyObject *self, PyObject *args) {
    int eid = 0;

    if (!PyArg_ParseTuple(args, "i", &eid))
        return 0;
    return Py_BuildValue("i", sp_close(eid));
}

static PyObject *cpy_sp_send(PyObject *self, PyObject *args) {
    int eid = 0;
    int rc = 0;
    Message *message = 0;

    if (!PyArg_ParseTuple(args, "iO", &eid, &message))
	return 0;
    if (!message->ubuf) {
	PyErr_BadInternalCall();
	return 0;
    }
    if ((rc = sp_send(eid, message->ubuf)) == 0) {
	message->ubuf = 0;
    }
    return Py_BuildValue("i", rc);
}

static PyObject *cpy_sp_recv(PyObject *self, PyObject *args) {
    PyObject *tuple = PyTuple_New(2);
    Message *message = (Message *)PyType_GenericAlloc(&MessageType, 0);
    int eid = 0;
    int rc = 0;
    char *ubuf = 0;

    if (!PyArg_ParseTuple(args, "i", &eid))
	return 0;
    if ((rc = sp_recv(eid, &ubuf)) == 0)
	message->ubuf = ubuf;
    PyTuple_SetItem(tuple, 0, Py_BuildValue("i", rc));
    PyTuple_SetItem(tuple, 1, (PyObject *)message);
    return tuple;
}

static PyObject *cpy_sp_add(PyObject *self, PyObject *args) {
    int eid = 0;
    int sockfd = 0;

    if (!PyArg_ParseTuple(args, "ii", &eid, &sockfd))
        return 0;
    return Py_BuildValue("i", sp_add(eid, sockfd));
}

static PyObject *cpy_sp_rm(PyObject *self, PyObject *args) {
    int eid = 0;
    int sockfd = 0;

    if (!PyArg_ParseTuple(args, "ii", &eid, &sockfd))
        return 0;
    return Py_BuildValue("i", sp_rm(eid, sockfd));
}

static PyObject *cpy_sp_setopt(PyObject *self, PyObject *args) {
    return 0;
}


static PyObject *cpy_sp_getopt(PyObject *self, PyObject *args) {
    return 0;
}

static PyObject *cpy_sp_listen(PyObject *self, PyObject *args) {
    int eid = 0;
    const char *sockaddr = 0;

    if (!PyArg_ParseTuple(args, "is", &eid, &sockaddr))
	return 0;
    return Py_BuildValue("i", sp_listen(eid, sockaddr));
}


static PyObject *cpy_sp_connect(PyObject *self, PyObject *args) {
    int eid = 0;
    const char *sockaddr = 0;

    if (!PyArg_ParseTuple(args, "is", &eid, &sockaddr))
	return 0;
    return Py_BuildValue("i", sp_connect(eid, sockaddr));
}

static PyMethodDef module_methods[] = {
    {"xallocubuf",      cpy_xallocubuf,     METH_VARARGS,  "alloc a user-space message"},
    {"xubuflen",        cpy_xubuflen,       METH_VARARGS,  "return the ubuf's length"},
    {"xfreeubuf",       cpy_xfreeubuf,      METH_VARARGS,  "free ubuf"},
    {"xsocket",         cpy_xsocket,        METH_VARARGS,  "create an socket"},
    {"xbind",           cpy_xbind,          METH_VARARGS,  "bind the sockaddr to the socket"},
    {"xlisten",         cpy_xlisten,        METH_VARARGS,  "listen the sockaddr"},
    {"xconnect",        cpy_xconnect,       METH_VARARGS,  "connecto to the sockaddr"},
    {"xaccept",         cpy_xaccept,        METH_VARARGS,  "accept an new socket"},
    {"xrecv",           cpy_xrecv,          METH_VARARGS,  "receive a message"},
    {"xsend",           cpy_xsend,          METH_VARARGS,  "send a message"},
    {"xclose",          cpy_xclose,         METH_VARARGS,  "close the socket"},
    {"xsetopt",         cpy_xsetopt,        METH_VARARGS,  "set socket options"},
    {"xgetopt",         cpy_xgetopt,        METH_VARARGS,  "get socket options"},
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

typedef struct {
    const char *name;
    int value;
} py_xio_constant;

static py_xio_constant xio_consts[] = {
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

void pyopen_xio() {
    PyObject *pyxio = Py_InitModule("xio", module_methods);
    int i;
    py_xio_constant *c;

    BUG_ON(PyType_Ready(&MessageType) < 0);
    Py_INCREF(&MessageType);
    PyModule_AddObject(pyxio, "Message", (PyObject *)&MessageType);
    for (i = 0; i < NELEM(xio_consts, py_xio_constant); i++) {
	c = &xio_consts[i];
	PyModule_AddIntConstant(pyxio, c->name, c->value);
    }
}
