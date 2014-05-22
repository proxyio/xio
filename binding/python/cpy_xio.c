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
#include <string.h>
#include "cpy_xio.h"

const static char MODULE_NAME[] = "_xio_cpy";

typedef struct {
    PyObject_HEAD
    void *ubuf;
} Message;

static void Message_dealloc(Message *self) {
    if (self->ubuf != NULL) {
        xfreeubuf(self->ubuf);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *Message_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyErr_Format(PyExc_TypeError,
                 "cannot create '%.100s' instances us xallocubuf instead",
                 type->tp_name);
    return NULL;
}

static PyMemberDef Message_members[] = {
    {NULL}
};

static PyMethodDef Message_methods[] = {
    {NULL}
};

int Message_getbuffer(Message *self, Py_buffer *view, int flags) {
    if (self->ubuf == NULL) {
	PyErr_BadInternalCall();
	return -1;
    }
    return PyBuffer_FillInfo(view, (PyObject *)self, self->ubuf, xubuflen(self->ubuf), 0, flags);
}

#ifndef IS_PY3K
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
#endif

static PyBufferProcs Message_bufferproces = {
#ifndef IS_PY3K
    (readbufferproc)     Message_getreadbuffer,
    (writebufferproc)    Message_getwritebuffer,
    (segcountproc)       Message_getsegcountproc,
    NULL,
#endif
    (getbufferproc)      Message_getbuffer,
    NULL
};

static PyObject *Message_repr(Message *self) {
    return PyUnicode_FromFormat("<_nanomsg_cpy.Message size %zu, address %p >",
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
    (destructor)Message_dealloc, /*tp_dealloc*/
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
    | Py_TPFLAGS_HAVE_NEWBUFFER
    | Py_TPFLAGS_IS_ABSTRACT,    /*tp_flags*/
    "nanomsg allocated message wrapper supporting buffer protocol",
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
        return NULL;
    message = (Message *)PyType_GenericAlloc(&MessageType, 0);
    if ((message->ubuf = xallocubuf(size)) == NULL) {
        Py_DECREF((PyObject*)message);
        Py_RETURN_NONE;
    }
    return (PyObject*)message;
}

static PyObject *cpy_xubuflen (PyObject *self, PyObject *args) {
    Message *message = (Message *)self;
    return Py_BuildValue("i", xubuflen(message->ubuf));
}

static PyObject *cpy_xsocket(PyObject *self, PyObject *args) {
    int pf = 0;
    int socktype = 0;

    if (!PyArg_ParseTuple(args, "ii", &pf, &socktype))
        return NULL;
    return Py_BuildValue("i", xsocket(pf, socktype));
}

static PyObject *cpy_xbind(PyObject *self, PyObject *args) {
    int fd = 0;
    const char *sockaddr = 0;

    if (!PyArg_ParseTuple(args, "is", &fd, &sockaddr))
        return NULL;
    return Py_BuildValue("i", xbind(fd, sockaddr));
}

static PyObject *cpy_xaccept(PyObject *self, PyObject *args) {
    int fd;

    if (!PyArg_ParseTuple(args, "i", &fd))
        return NULL;
    return Py_BuildValue("i", xaccept(fd));
}

static PyObject *cpy_xlisten(PyObject *self, PyObject *args) {
    const char *sockaddr = 0;

    if (!PyArg_ParseTuple(args, "s", &sockaddr))
        return NULL;
    return Py_BuildValue("i", xlisten(sockaddr));
}

static PyObject *cpy_xconnect(PyObject *self, PyObject *args) {
    const char *sockaddr = 0;

    if (!PyArg_ParseTuple(args, "s", &sockaddr))
        return NULL;
    return Py_BuildValue("i", xconnect(sockaddr));
}

static PyObject *cpy_xrecv(PyObject *self, PyObject *args) {
    return 0;
}

static PyObject *cpy_xsend(PyObject *self, PyObject *args) {
    return 0;
}

static PyObject *cpy_xclose(PyObject *self, PyObject *args) {
    int fd = 0;

    if (!PyArg_ParseTuple(args, "i", &fd))
        return NULL;
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
        return NULL;
    return Py_BuildValue("i", sp_endpoint(sp_family, sp_type));
}

static PyObject *cpy_sp_close(PyObject *self, PyObject *args) {
    int eid = 0;

    if (!PyArg_ParseTuple(args, "i", &eid))
        return NULL;
    return Py_BuildValue("i", sp_close(eid));
}

static PyObject *cpy_sp_send(PyObject *self, PyObject *args) {
    return 0;
}

static PyObject *cpy_sp_recv(PyObject *self, PyObject *args) {
    return 0;
}

static PyObject *cpy_sp_add(PyObject *self, PyObject *args) {
    int eid = 0;
    int sockfd = 0;

    if (!PyArg_ParseTuple(args, "ii", &eid, &sockfd))
        return NULL;
    return Py_BuildValue("i", sp_add(eid, sockfd));
}

static PyObject *cpy_sp_rm(PyObject *self, PyObject *args) {
    int eid = 0;
    int sockfd = 0;

    if (!PyArg_ParseTuple(args, "ii", &eid, &sockfd))
        return NULL;
    return Py_BuildValue("i", sp_rm(eid, sockfd));
}

static PyObject *cpy_sp_setopt(PyObject *self, PyObject *args) {
    return 0;
}


static PyObject *cpy_sp_getopt(PyObject *self, PyObject *args) {
    return 0;
}

static PyMethodDef module_methods[] = {
    {"xallocubuf",      cpy_xallocubuf,     METH_VARARGS,  "alloc a user-space message"},
    {"xubuflen",        cpy_xubuflen,       METH_VARARGS,  "return the ubuf's length"},
    {"xfreeubuf",       0,                  METH_VARARGS,  ""},
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
    {NULL, NULL, 0, NULL}
};

int cpyopen_xio () {
    return 0;
}
