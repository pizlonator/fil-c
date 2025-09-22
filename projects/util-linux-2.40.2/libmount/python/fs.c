/*
 * Python bindings for the libmount library.
 *
 * Copyright (C) 2013, Red Hat, Inc. All rights reserved.
 * Written by Ondrej Oprala and Karel Zak
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * TODO:
 * mnt_fs_match_{source,target}
 * mnt_fs_get_{attribute,option}
 */

#include "pylibmount.h"
#include <errno.h>

#define Fs_HELP "Fs(source=None, root=None, target=None, fstype=None, options=None, attributes=None, freq=0, passno=0)"

static PyMemberDef Fs_members[] = {
	{NULL}
};

static PyObject *Fs_get_tag(FsObject *self)
{
	const char *tag = NULL, *val = NULL;
	PyObject *result;

	if (mnt_fs_get_tag(self->fs, &tag, &val) != 0)
		return NULL;

	result = Py_BuildValue("(ss)", tag, val);
	if (!result)
		PyErr_SetString(PyExc_RuntimeError, CONSTRUCT_ERR);
	return result;
}

static PyObject *Fs_get_id(FsObject *self)
{
	return PyObjectResultInt(mnt_fs_get_id(self->fs));
}

static PyObject *Fs_get_parent_id(FsObject *self)
{
	return PyObjectResultInt(mnt_fs_get_parent_id(self->fs));
}

static PyObject *Fs_get_devno(FsObject *self)
{
	return PyObjectResultInt(mnt_fs_get_devno(self->fs));
}

static void _dump_debug_string(const char *lead, const char *s, char quote)
{
	/* PySys_WriteStdout() will automatically truncate any '%s' token
	 * longer than a certain length (documented as 1000 bytes, but we
	 * give ourselves some margin here just in case).  The only way I
	 * know to get around this is to print such strings in bite-sized
	 * chunks.
	 */
	static const unsigned int _PY_MAX_LEN = 900;
	static const char *_PY_MAX_LEN_FMT = "%.900s";
	unsigned int len;

	if (lead != NULL)
		PySys_WriteStdout("%s", lead);

	if (quote != 0)
		PySys_WriteStdout("%c", quote);

	for (len = strlen(s); len > _PY_MAX_LEN; len -= _PY_MAX_LEN, s += _PY_MAX_LEN)
		PySys_WriteStdout(_PY_MAX_LEN_FMT, s);

	if (len > 0)
		PySys_WriteStdout(_PY_MAX_LEN_FMT, s);

	if (quote != 0)
		PySys_WriteStdout("%c\n", quote);
	else
		PySys_WriteStdout("\n");
}

#define Fs_print_debug_HELP "print_debug()\n\n"
static PyObject *Fs_print_debug(FsObject *self)
{
	PySys_WriteStdout("------ fs: %p\n", self->fs);
	_dump_debug_string("source: ", mnt_fs_get_source(self->fs), 0);
	_dump_debug_string("target: ", mnt_fs_get_target(self->fs), 0);
	_dump_debug_string("fstype: ", mnt_fs_get_fstype(self->fs), 0);

	if (mnt_fs_get_options(self->fs))
		_dump_debug_string("optstr: ", mnt_fs_get_options(self->fs), 0);
	if (mnt_fs_get_vfs_options(self->fs))
		_dump_debug_string("VFS-optstr: ", mnt_fs_get_vfs_options(self->fs), 0);
	if (mnt_fs_get_fs_options(self->fs))
		_dump_debug_string("FS-opstr: ", mnt_fs_get_fs_options(self->fs), 0);
	if (mnt_fs_get_user_options(self->fs))
		_dump_debug_string("user-optstr: ", mnt_fs_get_user_options(self->fs), 0);
	if (mnt_fs_get_optional_fields(self->fs))
		_dump_debug_string("optional-fields: ", mnt_fs_get_optional_fields(self->fs), '\'');
	if (mnt_fs_get_attributes(self->fs))
		_dump_debug_string("attributes: ", mnt_fs_get_attributes(self->fs), 0);

	if (mnt_fs_get_root(self->fs))
		_dump_debug_string("root:   ", mnt_fs_get_root(self->fs), 0);

	if (mnt_fs_get_swaptype(self->fs))
		_dump_debug_string("swaptype: ", mnt_fs_get_swaptype(self->fs), 0);
	if (mnt_fs_get_size(self->fs))
		PySys_WriteStdout("size: %jd\n", mnt_fs_get_size(self->fs));
	if (mnt_fs_get_usedsize(self->fs))
		PySys_WriteStdout("usedsize: %jd\n", mnt_fs_get_usedsize(self->fs));
	if (mnt_fs_get_priority(self->fs))
		PySys_WriteStdout("priority: %d\n", mnt_fs_get_priority(self->fs));

	if (mnt_fs_get_bindsrc(self->fs))
		_dump_debug_string("bindsrc: ", mnt_fs_get_bindsrc(self->fs), 0);
	if (mnt_fs_get_freq(self->fs))
		PySys_WriteStdout("freq:   %d\n", mnt_fs_get_freq(self->fs));
	if (mnt_fs_get_passno(self->fs))
		PySys_WriteStdout("pass:   %d\n", mnt_fs_get_passno(self->fs));
	if (mnt_fs_get_id(self->fs))
		PySys_WriteStdout("id:     %d\n", mnt_fs_get_id(self->fs));
	if (mnt_fs_get_parent_id(self->fs))
		PySys_WriteStdout("parent: %d\n", mnt_fs_get_parent_id(self->fs));
	if (mnt_fs_get_devno(self->fs))
		PySys_WriteStdout("devno:  %d:%d\n", major(mnt_fs_get_devno(self->fs)),
						minor(mnt_fs_get_devno(self->fs)));
	if (mnt_fs_get_tid(self->fs))
		PySys_WriteStdout("tid:    %d\n", mnt_fs_get_tid(self->fs));
	if (mnt_fs_get_comment(self->fs))
		_dump_debug_string("comment: ", mnt_fs_get_comment(self->fs), '\'');
	return UL_IncRef(self);
}
/*
 ** Fs getters/setters
 */

static PyObject *Fs_get_comment(FsObject *self, void *closure __attribute__((unused)))
{
	return PyObjectResultStr(mnt_fs_get_comment(self->fs));
}

static int Fs_set_comment(FsObject *self, PyObject *value, void *closure __attribute__((unused)))
{
	char *comment = NULL;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(comment = pystos(value)))
		return -1;

	rc = mnt_fs_set_comment(self->fs, comment);
	if (rc) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}
/* source */
static PyObject *Fs_get_source(FsObject *self)
{
	return PyObjectResultStr(mnt_fs_get_source(self->fs));
}

static int Fs_set_source(FsObject *self, PyObject *value, void *closure __attribute__((unused)))
{
	char *source = NULL;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(source = pystos(value)))
		return -1;

	rc = mnt_fs_set_source(self->fs, source);
	if (rc) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}

static PyObject *Fs_get_srcpath(FsObject *self)
{
	return PyObjectResultStr(mnt_fs_get_srcpath(self->fs));
}

static PyObject *Fs_get_root(FsObject *self)
{
	return PyObjectResultStr(mnt_fs_get_root(self->fs));
}

static int Fs_set_root(FsObject *self, PyObject *value, void *closure __attribute__((unused)))
{
	char *root = NULL;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(root = pystos(value)))
		return -1;

	rc = mnt_fs_set_root(self->fs, root);
	if (rc) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}

static PyObject *Fs_get_target(FsObject *self)
{
	return PyObjectResultStr(mnt_fs_get_target(self->fs));
}

static int Fs_set_target(FsObject *self, PyObject *value, void *closure __attribute__((unused)))
{
	char *target = NULL;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(target = pystos(value)))
		return -1;

	rc = mnt_fs_set_target(self->fs, target);
	if (rc) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}

static PyObject *Fs_get_fstype(FsObject *self)
{
	return PyObjectResultStr(mnt_fs_get_fstype(self->fs));
}

static int Fs_set_fstype(FsObject *self, PyObject *value,
			void *closure __attribute__((unused)))
{
	char *fstype = NULL;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(fstype = pystos(value)))
		return -1;

	rc = mnt_fs_set_fstype(self->fs, fstype);
	if (rc) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}

static PyObject *Fs_get_options(FsObject *self)
{
	return PyObjectResultStr(mnt_fs_get_options(self->fs));
}

static int Fs_set_options(FsObject *self, PyObject *value,
			void *closure __attribute__((unused)))
{
	char *options = NULL;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(options = pystos(value)))
		return -1;

	rc = mnt_fs_set_options(self->fs, options);
	if (rc) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}

static PyObject *Fs_get_vfs_options(FsObject *self)
{
	return PyObjectResultStr(mnt_fs_get_vfs_options(self->fs));
}


static PyObject *Fs_get_optional_fields(FsObject *self)
{
	return PyObjectResultStr(mnt_fs_get_optional_fields(self->fs));
}


static PyObject *Fs_get_fs_options(FsObject *self)
{
	return PyObjectResultStr(mnt_fs_get_fs_options(self->fs));
}


static PyObject *Fs_get_user_options(FsObject *self)
{
	return PyObjectResultStr(mnt_fs_get_user_options(self->fs));
}


static PyObject *Fs_get_attributes(FsObject *self)
{
	return PyObjectResultStr(mnt_fs_get_attributes(self->fs));
}

static int Fs_set_attributes(FsObject *self, PyObject *value,
			void *closure __attribute__((unused)))
{
	char *attributes = NULL;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(attributes = pystos(value)))
		return -1;

	rc = mnt_fs_set_attributes(self->fs, attributes);
	if (rc) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}

static PyObject *Fs_get_freq(FsObject *self, void *closure __attribute__((unused)))
{
	return PyObjectResultInt(mnt_fs_get_freq(self->fs));
}

static int Fs_set_freq(FsObject *self, PyObject *value,
				void *closure __attribute__((unused)))
{
	int freq = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;

	}

	if (!PyLong_Check(value)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return -1;
	}

	freq = PyLong_AsLong(value);
	if (freq == -1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_RuntimeError, "type conversion failed");
		return -1;
	}
	return mnt_fs_set_freq(self->fs, freq);
}

static PyObject *Fs_get_passno(FsObject *self)
{
	return PyObjectResultInt(mnt_fs_get_passno(self->fs));
}

static int Fs_set_passno(FsObject *self, PyObject *value, void *closure __attribute__((unused)))
{
	int passno = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}

	if (!PyLong_Check(value)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return -1;
	}

	passno = PyLong_AsLong(value);
	if (passno == -1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_RuntimeError, "type conversion failed");
		return -1;
	}
	return mnt_fs_set_passno(self->fs, passno);
}

static PyObject *Fs_get_swaptype(FsObject *self)
{
	return PyObjectResultStr(mnt_fs_get_swaptype(self->fs));
}

static PyObject *Fs_get_size(FsObject *self)
{
	return PyObjectResultInt(mnt_fs_get_size(self->fs));
}

static PyObject *Fs_get_usedsize(FsObject *self)
{
	return PyObjectResultInt(mnt_fs_get_usedsize(self->fs));
}

static PyObject *Fs_get_priority(FsObject *self)
{
	return PyObjectResultInt(mnt_fs_get_priority(self->fs));
}

#define Fs_get_propagation_HELP "get_propagation(flags)\n\n\
Note that this function set flags to zero if not found any propagation flag\n\
in mountinfo file. The kernel default is MS_PRIVATE, this flag is not stored\n\
in the mountinfo file.\n\
\n\
Returns self or raises an exception in case of an error."
static PyObject *Fs_get_propagation(FsObject *self)
{
	unsigned long flags;
	int rc;

	rc =  mnt_fs_get_propagation(self->fs, &flags);
	return rc ? UL_RaiseExc(-rc) : PyObjectResultInt(flags);
}

static PyObject *Fs_get_tid(FsObject *self)
{
	return PyObjectResultInt(mnt_fs_get_tid(self->fs));
}

#define Fs_is_kernel_HELP "is_kernel()\n\nReturns 1 if the filesystem " \
			  "description is read from kernel e.g. /proc/mounts."
static PyObject *Fs_is_kernel(FsObject *self)
{
	return PyBool_FromLong(mnt_fs_is_kernel(self->fs));
}

#define Fs_is_netfs_HELP "is_netfs()\n\nReturns 1 if the filesystem is " \
			 "a network filesystem"
static PyObject *Fs_is_netfs(FsObject *self)
{
	return PyBool_FromLong(mnt_fs_is_netfs(self->fs));
}

#define Fs_is_pseudofs_HELP "is_pseudofs()\n\nReturns 1 if the filesystem is "\
			    "a pseudo fs type (proc, cgroups)"
static PyObject *Fs_is_pseudofs(FsObject *self)
{
	return PyBool_FromLong(mnt_fs_is_pseudofs(self->fs));
}

#define Fs_is_swaparea_HELP "is_swaparea()\n\nReturns 1 if the filesystem " \
			    "uses \"swap\" as a type"
static PyObject *Fs_is_swaparea(FsObject *self)
{
	return PyBool_FromLong(mnt_fs_is_swaparea(self->fs));
}

#define Fs_append_attributes_HELP "append_attributes(optstr)\n\n" \
				  "Appends mount attributes."
static PyObject *Fs_append_attributes(FsObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"optstr", NULL};
	char *optstr = NULL;
	int rc;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &optstr)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_fs_append_attributes(self->fs, optstr);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Fs_append_options_HELP "append_options(optstr)\n\n" \
			"Parses (splits) optstr and appends results to VFS, " \
			"FS and userspace lists of options."
static PyObject *Fs_append_options(FsObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"optstr", NULL};
	char *optstr = NULL;
	int rc;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &optstr)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_fs_append_options(self->fs, optstr);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Fs_prepend_attributes_HELP "prepend_attributes(optstr)\n\n" \
				   "Prepends mount attributes."
static PyObject *Fs_prepend_attributes(FsObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"optstr", NULL};
	char *optstr = NULL;
	int rc;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &optstr)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_fs_prepend_attributes(self->fs, optstr);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Fs_prepend_options_HELP "prepend_options(optstr)\n\n" \
			"Parses (splits) optstr and prepends results to VFS, " \
			"FS and userspace lists of options."
static PyObject *Fs_prepend_options(FsObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"optstr", NULL};
	char *optstr = NULL;
	int rc;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &optstr)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_fs_prepend_options(self->fs, optstr);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Fs_match_fstype_HELP "match_fstype(pattern)\n\n" \
		"pattern: filesystem name or comma delimited list(string) of names\n\n" \
		"The pattern list of filesystem can be prefixed with a global\n" \
		"\"no\" prefix to invert matching of the whole list. The \"no\" could\n" \
		"also be used for individual items in the pattern list. So,\n" \
		"\"nofoo,bar\" has the same meaning as \"nofoo,nobar\".\n" \
		"\"bar\" : \"nofoo,bar\"	-> False   (global \"no\" prefix)\n" \
		"\"bar\" : \"foo,bar\"		-> True\n" \
		"\"bar\" : \"foo,nobar\"	-> False\n\n" \
		"Returns True if type is matching, else False."
static PyObject *Fs_match_fstype(FsObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"pattern", NULL};
	char *pattern = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &pattern)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	return PyBool_FromLong(mnt_fs_match_fstype(self->fs, pattern));
}

#define Fs_match_options_HELP "match_options(options)\n\n" \
		"options: comma delimited list of options (and nooptions)\n" \
		"Returns True if fs type is matching to options else False."
static PyObject *Fs_match_options(FsObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"options", NULL};
	char *options = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &options)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	return PyBool_FromLong(mnt_fs_match_options(self->fs, options));
}

#define Fs_streq_srcpath_HELP "streq_srcpath(srcpath)\n\n" \
		"Compares fs source path with path. The tailing slash is ignored.\n" \
		"Returns True if fs source path equal to path, otherwise False."
static PyObject *Fs_streq_srcpath(FsObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"srcpath", NULL};
	char *srcpath = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &srcpath)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	return PyBool_FromLong(mnt_fs_streq_srcpath(self->fs, srcpath));
}

#define Fs_streq_target_HELP "streq_target(target)\n\n" \
		"Compares fs target path with path. The tailing slash is ignored.\n" \
		"See also Fs.match_target().\n" \
		"Returns True if fs target path equal to path, otherwise False."
static PyObject *Fs_streq_target(FsObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"target", NULL};
	char *target = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &target)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	return PyBool_FromLong(mnt_fs_streq_target(self->fs, target));
}

#define Fs_copy_fs_HELP "copy_fs(dest=None)\n\n" \
		"If dest is None, a new object is created, if any fs " \
		"field is already set, then the field is NOT overwritten."
static PyObject *Fs_copy_fs(FsObject *self, PyObject *args, PyObject *kwds);

static PyMethodDef Fs_methods[] = {
	{"get_propagation",	(PyCFunction)Fs_get_propagation, METH_NOARGS, Fs_get_propagation_HELP},
	{"mnt_fs_append_attributes",	(PyCFunction)Fs_append_attributes, METH_VARARGS|METH_KEYWORDS, Fs_append_attributes_HELP},
	{"append_options",	(PyCFunction)Fs_append_options, METH_VARARGS|METH_KEYWORDS, Fs_append_options_HELP},
	{"mnt_fs_prepend_attributes",	(PyCFunction)Fs_prepend_attributes, METH_VARARGS|METH_KEYWORDS, Fs_prepend_attributes_HELP},
	{"prepend_options",	(PyCFunction)Fs_prepend_options, METH_VARARGS|METH_KEYWORDS, Fs_prepend_options_HELP},
	{"copy_fs",		(PyCFunction)Fs_copy_fs, METH_VARARGS|METH_KEYWORDS, Fs_copy_fs_HELP},
	{"is_kernel",		(PyCFunction)Fs_is_kernel, METH_NOARGS, Fs_is_kernel_HELP},
	{"is_netfs",		(PyCFunction)Fs_is_netfs, METH_NOARGS, Fs_is_netfs_HELP},
	{"is_pseudofs",		(PyCFunction)Fs_is_pseudofs, METH_NOARGS, Fs_is_pseudofs_HELP},
	{"is_swaparea",		(PyCFunction)Fs_is_swaparea, METH_NOARGS, Fs_is_swaparea_HELP},
	{"match_fstype",	(PyCFunction)Fs_match_fstype, METH_VARARGS|METH_KEYWORDS, Fs_match_fstype_HELP},
	{"match_options",	(PyCFunction)Fs_match_options, METH_VARARGS|METH_KEYWORDS, Fs_match_options_HELP},
	{"streq_srcpath",	(PyCFunction)Fs_streq_srcpath, METH_VARARGS|METH_KEYWORDS, Fs_streq_srcpath_HELP},
	{"streq_target",	(PyCFunction)Fs_streq_target, METH_VARARGS|METH_KEYWORDS, Fs_streq_target_HELP},
	{"print_debug",		(PyCFunction)Fs_print_debug, METH_NOARGS, Fs_print_debug_HELP},
	{NULL}
};

static void Fs_destructor(FsObject *self)
{
	DBG(FS, pymnt_debug_h(self->fs, "destructor py-obj: %p, py-refcnt=%d",
				self, (int) ((PyObject *) self)->ob_refcnt));
	mnt_unref_fs(self->fs);
	PyFree(self);
}

static PyObject *Fs_new(PyTypeObject *type, PyObject *args __attribute__((unused)),
		PyObject *kwds __attribute__((unused)))
{
	FsObject *self = (FsObject*)type->tp_alloc(type, 0);

	if (self) {
		self->fs = NULL;
		DBG(FS, pymnt_debug_h(self, "new"));
	}
	return (PyObject *) self;
}

static int Fs_init(FsObject *self, PyObject *args, PyObject *kwds)
{
	char *source = NULL, *root = NULL, *target = NULL;
	char *fstype = NULL, *options = NULL, *attributes =NULL;
	int freq = 0; int passno = 0;
	int rc = 0;
	char *kwlist[] = {
		"source", "root", "target",
		"fstype", "options", "attributes",
		"freq", "passno", NULL
	};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ssssssii", kwlist,
				&source, &root, &target, &fstype, &options,
				&attributes, &freq, &passno)) {
		PyErr_SetString(PyExc_TypeError, "Invalid type");
		return -1;
	}

	DBG(FS, pymnt_debug_h(self, "init"));

	if (self->fs)
		mnt_unref_fs(self->fs);

	self->fs = mnt_new_fs();		/* new FS with refcount=1 */

	if (source && (rc = mnt_fs_set_source(self->fs, source))) {
		PyErr_SetString(PyExc_MemoryError, MEMORY_ERR);
		return rc;
	}
	if (root && (rc = mnt_fs_set_root(self->fs, root))) {
		PyErr_SetString(PyExc_MemoryError, MEMORY_ERR);
		return rc;
	}
	if (target && (rc = mnt_fs_set_target(self->fs, target))) {
		PyErr_SetString(PyExc_MemoryError, MEMORY_ERR);
		return rc;
	}
	if (fstype && (rc = mnt_fs_set_fstype(self->fs, fstype))) {
		PyErr_SetString(PyExc_MemoryError, MEMORY_ERR);
		return rc;
	}
	if (options && (rc = mnt_fs_set_options(self->fs, options))) {
		PyErr_SetString(PyExc_MemoryError, MEMORY_ERR);
		return rc;
	}
	if (attributes && (rc = mnt_fs_set_attributes(self->fs, attributes))) {
		PyErr_SetString(PyExc_MemoryError, MEMORY_ERR);
		return rc;
	}

	mnt_fs_set_freq(self->fs, freq);
	mnt_fs_set_passno(self->fs, passno);
	mnt_fs_set_userdata(self->fs, self); /* store a pointer to self, convenient when resetting the table */
	return 0;
}

/*
 * missing:
 * attribute
 * option
 */
static PyGetSetDef Fs_getseters[] = {
	{"id",		(getter)Fs_get_id, NULL, "mountinfo[1]: ID", NULL},
	{"parent",	(getter)Fs_get_parent_id, NULL, "mountinfo[2]: parent", NULL},
	{"devno",	(getter)Fs_get_devno, NULL, "mountinfo[3]: st_dev", NULL},
	{"comment",	(getter)Fs_get_comment, (setter)Fs_set_comment, "fstab entry comment", NULL},
	{"source",	(getter)Fs_get_source, (setter)Fs_set_source, "fstab[1], mountinfo[10], swaps[1]: source dev, file, dir or TAG", NULL},
	{"srcpath",	(getter)Fs_get_srcpath, NULL, "mount source path or NULL in case of error or when the path is not defined.", NULL},
	{"root",	(getter)Fs_get_root, (setter)Fs_set_root, "mountinfo[4]: root of the mount within the FS", NULL},
	{"target",	(getter)Fs_get_target, (setter)Fs_set_target, "mountinfo[5], fstab[2]: mountpoint", NULL},
	{"fstype",	(getter)Fs_get_fstype, (setter)Fs_set_fstype, "mountinfo[9], fstab[3]: filesystem type", NULL},
	{"options",	(getter)Fs_get_options, (setter)Fs_set_options, "fstab[4]: merged options", NULL},
	{"vfs_options",	(getter)Fs_get_vfs_options, NULL, "mountinfo[6]: fs-independent (VFS) options", NULL},
	{"opt_fields",	(getter)Fs_get_optional_fields, NULL, "mountinfo[7]: optional fields", NULL},
	{"fs_options",	(getter)Fs_get_fs_options, NULL, "mountinfo[11]: fs-dependent options", NULL},
	{"usr_options",	(getter)Fs_get_user_options, NULL, "userspace mount options", NULL},
	{"attributes",	(getter)Fs_get_attributes, (setter)Fs_set_attributes, "mount attributes", NULL},
	{"freq",	(getter)Fs_get_freq, (setter)Fs_set_freq, "fstab[5]: dump frequency in days", NULL},
	{"passno",	(getter)Fs_get_passno, (setter)Fs_set_passno, "fstab[6]: pass number on parallel fsck", NULL},
	{"swaptype",	(getter)Fs_get_swaptype, NULL, "swaps[2]: device type", NULL},
	{"size",	(getter)Fs_get_size, NULL, "saps[3]: swaparea size", NULL},
	{"usedsize",	(getter)Fs_get_usedsize, NULL, "swaps[4]: used size", NULL},
	{"priority",	(getter)Fs_get_priority, NULL, "swaps[5]: swap priority", NULL},
	{"tag",		(getter)Fs_get_tag, NULL, "(Name, Value)", NULL},
	{"tid",		(getter)Fs_get_tid, NULL, "/proc/<tid>/mountinfo, otherwise zero", NULL},
	{NULL}
};

static PyObject *Fs_repr(FsObject *self)
{
	const char *src = mnt_fs_get_source(self->fs),
		   *tgt = mnt_fs_get_target(self->fs),
		   *type = mnt_fs_get_fstype(self->fs);

	return PyUnicode_FromFormat(
			"<libmount.Fs object at %p, "
			"source=%s, target=%s, fstype=%s>",
			self,
			src ? src : "None",
			tgt ? tgt : "None",
			type ? type : "None");
}

PyObject *PyObjectResultFs(struct libmnt_fs *fs)
{
	FsObject *result;

	if (!fs) {
		PyErr_SetString(LibmountError, "internal exception");
		return NULL;
	}

	result = mnt_fs_get_userdata(fs);
	if (result) {
		Py_INCREF(result);
		DBG(FS, pymnt_debug_h(fs, "result py-obj %p: already exists, py-refcnt=%d",
				result, (int) ((PyObject *) result)->ob_refcnt));
		return (PyObject *) result;
	}

	/* Creating an encapsulating object: increment the refcount, so that code
	 * such as tab.next_fs() doesn't call the destructor, which would free
	 * our fs struct as well
	 */
	result = PyObject_New(FsObject, &FsType);
	if (!result) {
		UL_RaiseExc(ENOMEM);
		return NULL;
	}

	Py_INCREF(result);
	mnt_ref_fs(fs);

	DBG(FS, pymnt_debug_h(fs, "result py-obj %p new, py-refcnt=%d",
				result, (int) ((PyObject *) result)->ob_refcnt));
	result->fs = fs;
	mnt_fs_set_userdata(fs, result);
	return (PyObject *) result;
}

static PyObject *Fs_copy_fs(FsObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *dest = NULL;
	char *kwlist[] = {"dest", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist, &dest)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	if (PyObject_TypeCheck(dest, &FsType)) {	/* existing object passed as argument */
		if (!mnt_copy_fs(((FsObject *)dest)->fs, self->fs))
			return NULL;
		DBG(FS, pymnt_debug_h(dest, "copy data"));
		return (PyObject *)dest;

	}

	if (dest == Py_None) {			/* create new object */
		FsObject *result = PyObject_New(FsObject, &FsType);

		DBG(FS, pymnt_debug_h(result, "new copy"));
		result->fs = mnt_copy_fs(NULL, self->fs);
		mnt_fs_set_userdata(result->fs, result);	/* keep a pointer to encapsulating object */
		return (PyObject *)result;
	}

	PyErr_SetString(PyExc_TypeError, ARG_ERR);
	return NULL;
}


PyTypeObject FsType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libmount.Fs",
	.tp_basicsize = sizeof(FsObject),
	.tp_dealloc = (destructor)Fs_destructor,
	.tp_repr = (reprfunc)Fs_repr,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_doc = Fs_HELP,
	.tp_methods = Fs_methods,
	.tp_members = Fs_members,
	.tp_getset = Fs_getseters,
	.tp_init = (initproc)Fs_init,
	.tp_new = Fs_new,
};

void FS_AddModuleObject(PyObject *mod)
{
	if (PyType_Ready(&FsType) < 0)
		return;

	DBG(FS, pymnt_debug("add to module"));
	Py_INCREF(&FsType);
	PyModule_AddObject(mod, "Fs", (PyObject *)&FsType);
}

