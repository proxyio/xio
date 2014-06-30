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

typedef struct {
	PyObject_HEAD;
	char *ubuf;
} Msghdr;

static PyObject *Msghdr_new (PyTypeObject *type, PyObject *args, PyObject *kwds);
static void Msghdr_dealloc (Msghdr *self);

static PyMemberDef Msghdr_members[] = {
	{ NULL }       /* Sentinel */
};

static PyMethodDef Msghdr_methods[] = {
	{ NULL }        /* Sentinel */
};

static PyObject *Msghdr_repr (Msghdr *self);
static PyObject *Msghdr_str (Msghdr * self);

static PyTypeObject Msghdr_Type = {
	PyVarObject_HEAD_INIT (NULL, 0)
	"_xio_cpy.Msghdr",           /*tp_name*/
	sizeof (Msghdr),             /*tp_basicsize*/
	0,                           /*tp_itemsize*/
	(destructor) Msghdr_dealloc, /*tp_dealloc*/
	0,                           /*tp_print*/
	0,                           /*tp_getattr*/
	0,                           /*tp_setattr*/
	0,                           /*tp_compare*/
	(reprfunc) Msghdr_repr,      /*tp_repr*/
	0,                           /*tp_as_number*/
	0,                           /*tp_as_sequence*/
	0,                           /*tp_as_mapping*/
	0,                           /*tp_hash */
	0,                           /*tp_call*/
	(reprfunc) Msghdr_str,       /*tp_str*/
	0,                           /*tp_getattro*/
	0,                           /*tp_setattro*/
	0,                           /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	"allocated message wrapper supporting buffer protocol",
	/* tp_doc */
	0,                           /* tp_traverse */
	0,                           /* tp_clear */
	0,                           /* tp_richcompare */
	0,                           /* tp_weaklistoffset */
	0,                           /* tp_iter */
	0,                           /* tp_iternext */
	Msghdr_methods,              /* tp_methods */
	Msghdr_members,              /* tp_members */
	0,                           /* tp_getset */
	0,                           /* tp_base */
	0,                           /* tp_dict */
	0,                           /* tp_descr_get */
	0,                           /* tp_descr_set */
	0,                           /* tp_dictoffset */
	0,                           /* tp_init */
	0,                           /* tp_alloc */
	Msghdr_new,                  /* tp_new */
};

static PyObject *Msghdr_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	Msghdr *hdr = (Msghdr *) PyType_GenericAlloc (&Msghdr_Type, 0);
	return (PyObject *) hdr;
}

static void Msghdr_dealloc (Msghdr *self)
{
	if (self->ubuf)
		ubuf_free (self->ubuf);
	self->ob_type->tp_free ((PyObject *)self);
}

static PyObject *Msghdr_repr (Msghdr *self)
{
	return PyUnicode_FromFormat ("<_xio_cpy.Msghdr size %zu, address %p >",
				     sizeof (Msghdr), self);
}

static PyObject *Msghdr_str (Msghdr *self)
{
	return 0;
}





typedef struct {
	PyObject_HEAD;
	PyObject *data;
	PyObject *hdr;
} Msg;

static PyObject *Msg_new (PyTypeObject *type, PyObject *args, PyObject *kwds);
static void Msg_dealloc (Msg *self);

static PyMemberDef Msg_members[] = {
	{ NULL }       /* Sentinel */
};

static PyMethodDef Msg_methods[] = {
	{ NULL }        /* Sentinel */
};

static PyObject *Msg_getdata(Msg *self, void *closure)
{
	Py_INCREF(self->data);
	return self->data;
}

static int Msg_setdata(Msg *self, PyObject *value, void *closure)
{
	if (value == NULL) {
		PyErr_SetString(PyExc_TypeError, "Cannot delete the data attribute");
		return -1;
	}

	Py_DECREF(self->data);
	Py_INCREF(value);
	self->data = value;
	return 0;
}

static PyObject *Msg_gethdr(Msg *self, void *closure)
{
	Py_INCREF(self->hdr);
	return self->hdr;
}

static int Msg_sethdr(Msg *self, PyObject *value, void *closure)
{
	if (value == NULL) {
		PyErr_SetString(PyExc_TypeError, "Cannot delete the hdr attribute");
		return -1;
	}

	Py_DECREF(self->hdr);
	Py_INCREF(value);
	self->hdr = value;
	return 0;
}

static PyGetSetDef Msg_getseters[] = {
	{"data", (getter) Msg_getdata, (setter) Msg_setdata, "data", NULL},
	{"hdr", (getter) Msg_gethdr, (setter) Msg_sethdr, "hdr", NULL},
	{NULL}  /* Sentinel */
};


static PyObject *Msg_repr (Msg *self);
static PyObject *Msg_str (Msg * self);

static PyTypeObject Msg_Type = {
	PyVarObject_HEAD_INIT (NULL, 0)
	"_xio_cpy.Msg",              /*tp_name*/
	sizeof (Msg),                /*tp_basicsize*/
	0,                           /*tp_itemsize*/
	(destructor) Msg_dealloc,    /*tp_dealloc*/
	0,                           /*tp_print*/
	0,                           /*tp_getattr*/
	0,                           /*tp_setattr*/
	0,                           /*tp_compare*/
	(reprfunc) Msg_repr,         /*tp_repr*/
	0,                           /*tp_as_number*/
	0,                           /*tp_as_sequence*/
	0,                           /*tp_as_mapping*/
	0,                           /*tp_hash */
	0,                           /*tp_call*/
	(reprfunc) Msg_str,          /*tp_str*/
	0,                           /*tp_getattro*/
	0,                           /*tp_setattro*/
	0,                           /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	"allocated message wrapper supporting buffer protocol",
	/* tp_doc */
	0,                           /* tp_traverse */
	0,                           /* tp_clear */
	0,                           /* tp_richcompare */
	0,                           /* tp_weaklistoffset */
	0,                           /* tp_iter */
	0,                           /* tp_iternext */
	Msg_methods,                 /* tp_methods */
	Msg_members,                 /* tp_members */
	Msg_getseters,               /* tp_getset */
	0,                           /* tp_base */
	0,                           /* tp_dict */
	0,                           /* tp_descr_get */
	0,                           /* tp_descr_set */
	0,                           /* tp_dictoffset */
	0,                           /* tp_init */
	0,                           /* tp_alloc */
	Msg_new,                     /* tp_new */
};

static PyObject *Msg_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	Msg *msg = (Msg *) PyType_GenericAlloc (&Msg_Type, 0);
	msg->data = PyString_FromString ("");
	msg->hdr = PyString_FromString ("");
	return (PyObject *) msg;
}

static void Msg_dealloc (Msg *self)
{
	Py_XDECREF (self->data);
	Py_XDECREF (self->hdr);
	self->ob_type->tp_free ((PyObject *)self);
}

static PyObject *Msg_repr (Msg *self)
{
	return PyUnicode_FromFormat ("<_xio_cpy.Msg size %zu, address %p >",
				     sizeof (Msg), self);
}

static PyObject *Msg_str (Msg *self)
{
	return self->ob_type->tp_str (self->data);
}
