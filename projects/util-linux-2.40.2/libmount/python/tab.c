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
#include "pylibmount.h"

static PyMemberDef Table_members[] = {
	{ NULL }
};

static int Table_set_parser_errcb(TableObject *self, PyObject *func,
				  void *closure __attribute__((unused)))
{
	PyObject *tmp;

	if (!func) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}

	if (!PyCallable_Check(func))
		return -1;

	tmp = self->errcb;
	Py_INCREF(func);
	self->errcb = func;
	Py_XDECREF(tmp);
	return 0;
}

static PyObject *Table_get_intro_comment(TableObject *self,
				void *closure __attribute__((unused)))
{
	return PyObjectResultStr(mnt_table_get_intro_comment(self->tab));
}

static int Table_set_intro_comment(TableObject *self, PyObject *value,
				void *closure __attribute__((unused)))
{
	char *comment = NULL;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(comment = pystos(value)))
		return -1;

	if ((rc = mnt_table_set_intro_comment(self->tab, comment))) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}

static PyObject *Table_get_trailing_comment(TableObject *self,
				void *closure __attribute__((unused)))
{
	return PyObjectResultStr(mnt_table_get_trailing_comment(self->tab));
}

static int Table_set_trailing_comment(TableObject *self, PyObject *value,
				void *closure __attribute__((unused)))
{
	char *comment = NULL;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(comment = pystos(value)))
		return -1;

	if ((rc = mnt_table_set_trailing_comment(self->tab, comment))) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}

#define Table_enable_comments_HELP "enable_comments(enable)\n\n" \
	"Enables parsing of comments.\n\n" \
	"The initial (intro) file comment is accessible by\n" \
	"Tab.intro_comment. The intro and the comment of the first fstab" \
	"entry has to be separated by blank line.  The filesystem comments are\n" \
	"accessible by Fs.comment. The tailing fstab comment is accessible\n" \
	"by Tab.trailing_comment.\n" \
	"\n" \
	"<informalexample>\n" \
	"<programlisting>\n" \
	"#\n" \
	"# Intro comment\n" \
	"#\n" \
	"\n" \
	"# this comments belongs to the first fs\n" \
	"LABEL=foo /mnt/foo auto defaults 1 2\n" \
	"# this comments belongs to the second fs\n" \
	"LABEL=bar /mnt/bar auto defaults 1 2 \n" \
	"# tailing comment\n" \
	"</programlisting>\n" \
	"</informalexample>"
static PyObject *Table_enable_comments(TableObject *self, PyObject *args,
					PyObject *kwds)
{
	int enable = 0;
	char *kwlist[] = {"enable", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &enable)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	mnt_table_enable_comments(self->tab, enable);
	Py_INCREF(self);
	return (PyObject *)self;
}

#define Table_replace_file_HELP "replace_file(filename)\n\n" \
		"This function replaces filename with the new content from TableObject."
static PyObject *Table_replace_file(TableObject *self, PyObject *args, PyObject *kwds)
{
	int rc;
	char *filename = NULL;
	char *kwlist[] = {"filename", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &filename)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_table_replace_file(self->tab, filename);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Table_write_file_HELP "write_file(path)\n\n" \
		"This function writes tab to file(stream)"
static PyObject *Table_write_file(TableObject *self, PyObject *args, PyObject *kwds)
{
	int rc;
	//PyObject *stream = NULL;
	FILE *f = NULL;
	char *path = NULL;
	char *kwlist[] = {"path", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist,
					&path)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	if (!(f = fopen(path, "w")))
		return UL_RaiseExc(errno);
	rc = mnt_table_write_file(self->tab, f);
	fclose(f);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Table_find_devno_HELP "find_devno(devno, [direction])\n\n" \
		"Note that zero could be valid device number for root pseudo " \
		"filesystem (e.g. tmpfs)\n" \
		"Returns a tab entry or None"
static PyObject *Table_find_devno(TableObject *self, PyObject *args, PyObject *kwds)
{
	dev_t devno;
	int direction = MNT_ITER_BACKWARD;
	char *kwlist[] = {"devno", "direction", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|i", kwlist, &devno, &direction)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	return PyObjectResultFs(mnt_table_find_devno(self->tab, devno, direction));
}

#define Table_find_mountpoint_HELP "find_mountpoint(path, [direction])\n\n" \
		"Returns a tab entry or None."
static PyObject *Table_find_mountpoint(TableObject *self, PyObject *args, PyObject *kwds)
{
	char *path;
	int direction = MNT_ITER_BACKWARD;
	char *kwlist[] = {"path", "direction", NULL};

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "s|i", kwlist, &path, &direction)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	return PyObjectResultFs(mnt_table_find_mountpoint(self->tab, path, direction));
}

#define Table_find_pair_HELP "find_pair(source, target, [direction])\n\n" \
		"Returns a tab entry or None."
static PyObject *Table_find_pair(TableObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"source", "target", "direction", NULL};
	char *source;
	char *target;
	int direction = MNT_ITER_BACKWARD;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ss|i", kwlist, &source, &target, &direction)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	return PyObjectResultFs(mnt_table_find_pair(self->tab, source, target, direction));
}

#define Table_find_source_HELP "find_source(source, [direction])\n\n" \
		"Returns a tab entry or None."
static PyObject *Table_find_source(TableObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"source", "direction", NULL};
	char *source;
	int direction = MNT_ITER_BACKWARD;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|i", kwlist, &source, &direction)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	return PyObjectResultFs(mnt_table_find_source(self->tab, source, direction));
}

#define Table_find_target_HELP "find_target(target, [direction])\n\n" \
		"Try to lookup an entry in given tab, possible are three iterations, first\n" \
		"with path, second with realpath(path) and third with realpath(path)\n" \
		"against realpath(fs->target). The 2nd and 3rd iterations are not performed\n" \
		"when tb cache is not set (cache not implemented yet).\n" \
		"\n" \
		"Returns a tab entry or None."
static PyObject *Table_find_target(TableObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"target", "direction", NULL};
	char *target;
	int direction = MNT_ITER_BACKWARD;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|i", kwlist, &target, &direction)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	return PyObjectResultFs(mnt_table_find_target(self->tab, target, direction));
}

#define Table_find_srcpath_HELP "find_srcpath(srcpath, [direction])\n\n" \
		"Try to lookup an entry in given tab, possible are four iterations, first\n" \
		"with path, second with realpath(path), third with tags (LABEL, UUID, ..)\n" \
		"from path and fourth with realpath(path) against realpath(entry->srcpath).\n" \
		"\n" \
		"The 2nd, 3rd and 4th iterations are not performed when tb cache is not\n" \
		"set (not implemented yet).\n" \
		"\n" \
		"Note that None is a valid source path; it will be replaced with \"none\". The\n" \
		"\"none\" is used in /proc/{mounts,self/mountinfo} for pseudo filesystems.\n" \
		"\n" \
		"Returns a tab entry or None."
static PyObject *Table_find_srcpath(TableObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"srcpath", "direction", NULL};
	char *srcpath;
	int direction = MNT_ITER_BACKWARD;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|i", kwlist, &srcpath, &direction)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	return PyObjectResultFs(mnt_table_find_srcpath(self->tab, srcpath, direction));
}

#define Table_find_tag_HELP "find_tag(tag, val, [direction])\n\n" \
		"Try to lookup an entry in given tab, first attempt is lookup by tag and\n" \
		"val, for the second attempt the tag is evaluated (converted to the device\n" \
		"name) and Tab.find_srcpath() is performed. The second attempt is not\n" \
		"performed when tb cache is not set (not implemented yet).\n" \
		"\n" \
		"Returns a tab entry or NULL."
static PyObject *Table_find_tag(TableObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"tag", "val", "direction", NULL};
	char *tag;
	char *val;
	int direction = MNT_ITER_BACKWARD;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ss|i", kwlist, &tag, &val, &direction)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	return PyObjectResultFs(mnt_table_find_tag(self->tab, tag, val, direction));
}

static PyObject *Table_get_nents(TableObject *self)
{
	return PyObjectResultInt(mnt_table_get_nents(self->tab));
}

#define Table_is_fs_mounted_HELP "is_fs_mounted(fstab_fs)\n\n" \
		"Checks if the fstab_fs entry is already in the tb table. The \"swap\" is\n" \
		"ignored. This function explicitly compares source, target and root of the\n" \
		"filesystems.\n" \
		"\n" \
		"Note that source and target are canonicalized only if a cache for tb is\n" \
		"defined (not implemented yet). The target canonicalization may\n" \
		"trigger automount on autofs mountpoints!\n" \
		"\n" \
		"Don't use it if you want to know if a device is mounted, just use\n" \
		"Tab.find_source() for the device.\n" \
		"\n" \
		"This function is designed mostly for \"mount -a\".\n" \
		"\n" \
		"Returns a boolean value."
static PyObject *Table_is_fs_mounted(TableObject *self, PyObject *args, PyObject *kwds)
{
	FsObject *fs;
	char *kwlist[] = {"fstab_fs", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", kwlist, &FsType, &fs)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	return PyBool_FromLong(mnt_table_is_fs_mounted(self->tab, fs->fs));
}

#define Table_parse_file_HELP "parse_file(file)\n\n" \
		"Parses whole table (e.g. /etc/mtab) and appends new records to the tab.\n" \
		"\n" \
		"The libmount parser ignores broken (syntax error) lines, these lines are\n" \
		"reported to caller by errcb() function (see Tab.parser_errcb).\n" \
		"\n" \
		"Returns self or raises an exception in case of an error."
static PyObject *Table_parse_file(TableObject *self, PyObject* args, PyObject *kwds)
{
	int rc;
	char *file = NULL;
	char *kwlist[] = {"file", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &file)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_table_parse_file(self->tab, file);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Table_parse_fstab_HELP "parse_fstab([fstab])\n\n" \
		"This function parses /etc/fstab and appends new lines to the tab. If the\n" \
		"filename is a directory then Tab.parse_dir() is called.\n" \
		"\n" \
		"See also Tab.parser_errcb.\n" \
		"\n" \
		"Returns self or raises an exception in case of an error."
static PyObject *Table_parse_fstab(TableObject *self, PyObject* args, PyObject *kwds)
{
	int rc;
	char *fstab = NULL;
	char *kwlist[] = {"fstab", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|s", kwlist, &fstab)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_table_parse_fstab(self->tab, fstab);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Table_parse_mtab_HELP "parse_mtab([mtab])\n\n" \
		"This function parses /etc/mtab or /proc/self/mountinfo\n" \
		"/run/mount/utabs or /proc/mounts.\n" \
		"\n" \
		"See also Tab.parser_errcb().\n" \
		"\n" \
		"Returns self or raises an exception in case of an error."
static PyObject *Table_parse_mtab(TableObject *self, PyObject* args, PyObject *kwds)
{
	int rc;
	char *mtab = NULL;
	char *kwlist[] = {"mtab", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|s", kwlist, &mtab)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_table_parse_mtab(self->tab, mtab);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Table_parse_dir_HELP "parse_dir(dir)\n\n" \
		"The directory:\n" \
		"- files are sorted by strverscmp(3)\n" \
		"- files that start with \".\" are ignored (e.g. \".10foo.fstab\")\n" \
		"- files without the \".fstab\" extension are ignored\n" \
		"\n" \
		"Returns self or raises an exception in case of an error."
static PyObject *Table_parse_dir(TableObject *self, PyObject* args, PyObject *kwds)
{
	int rc;
	char *dir = NULL;
	char *kwlist[] = {"dir", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &dir)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_table_parse_dir(self->tab, dir);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Table_parse_swaps_HELP "parse_swaps(swaps)\n\n" \
		"This function parses /proc/swaps and appends new lines to the tab"
static PyObject *Table_parse_swaps(TableObject *self, PyObject* args, PyObject *kwds)
{
	int rc;
	char *swaps = NULL;
	char *kwlist[] = {"swaps", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &swaps)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_table_parse_swaps(self->tab, swaps);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Table_add_fs_HELP "add_fs(fs)\n\nAdds a new entry to tab.\n" \
		"Returns self or raises an exception in case of an error."

static PyObject *Table_add_fs(TableObject *self, PyObject* args, PyObject *kwds)
{
	int rc;
	FsObject *fs = NULL;
	char *kwlist[] = {"fs", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", kwlist, &FsType, &fs)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	Py_INCREF(fs);
	rc = mnt_table_add_fs(self->tab, fs->fs);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Table_remove_fs_HELP "remove_fs(fs)\n\n" \
		"Returns self or raises an exception in case of an error."
static PyObject *Table_remove_fs(TableObject *self, PyObject* args, PyObject *kwds)
{
	int rc;
	FsObject *fs = NULL;
	char *kwlist[] = {"fs", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", kwlist, &FsType, &fs)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_table_remove_fs(self->tab, fs->fs);
	Py_DECREF(fs);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Table_next_fs_HELP "next_fs()\n\n" \
		"Returns the next Fs on success, raises an exception in case " \
		"of an error and None at end of list.\n" \
		"\n" \
		"Example:\n" \
		"<informalexample>\n" \
		"<programlisting>\n" \
		"import libmount\n" \
		"import functools\n" \
		"for fs in iter(functools.partial(tb.next_fs), None):\n" \
		"    dir = Fs.target\n" \
		"    print \"mount point: {:s}\".format(dir)\n" \
		"\n" \
		"</programlisting>\n" \
		"</informalexample>\n" \
		"\n" \
		"lists all mountpoints from fstab in backward order."
static PyObject *Table_next_fs(TableObject *self)
{
	struct libmnt_fs *fs;
	int rc;

	/* Reset the builtin iterator after reaching the end of the list */
	rc = mnt_table_next_fs(self->tab, self->iter, &fs);
	if (rc == 1) {
		mnt_reset_iter(self->iter, MNT_ITER_FORWARD);
		Py_RETURN_NONE;
	}

	if (rc)
		return UL_RaiseExc(-rc);

	return PyObjectResultFs(fs);
}

static PyMethodDef Table_methods[] = {
	{"enable_comments", (PyCFunction)Table_enable_comments, METH_VARARGS|METH_KEYWORDS, Table_enable_comments_HELP},
	{"find_pair", (PyCFunction)Table_find_pair, METH_VARARGS|METH_KEYWORDS, Table_find_pair_HELP},
	{"find_source", (PyCFunction)Table_find_source, METH_VARARGS|METH_KEYWORDS, Table_find_source_HELP},
	{"find_srcpath", (PyCFunction)Table_find_srcpath, METH_VARARGS|METH_KEYWORDS, Table_find_srcpath_HELP},
	{"find_tag", (PyCFunction)Table_find_tag, METH_VARARGS|METH_KEYWORDS, Table_find_tag_HELP},
	{"find_target", (PyCFunction)Table_find_target, METH_VARARGS|METH_KEYWORDS, Table_find_target_HELP},
	{"find_devno", (PyCFunction)Table_find_devno, METH_VARARGS|METH_KEYWORDS, Table_find_devno_HELP},
	{"find_mountpoint", (PyCFunction)Table_find_mountpoint, METH_VARARGS|METH_KEYWORDS, Table_find_mountpoint_HELP},
	{"parse_file", (PyCFunction)Table_parse_file, METH_VARARGS|METH_KEYWORDS, Table_parse_file_HELP},
	{"parse_fstab", (PyCFunction)Table_parse_fstab, METH_VARARGS|METH_KEYWORDS, Table_parse_fstab_HELP},
	{"parse_mtab", (PyCFunction)Table_parse_mtab, METH_VARARGS|METH_KEYWORDS, Table_parse_mtab_HELP},
	{"parse_dir", (PyCFunction)Table_parse_dir, METH_VARARGS|METH_KEYWORDS, Table_parse_dir_HELP},
	{"parse_swaps", (PyCFunction)Table_parse_swaps, METH_VARARGS|METH_KEYWORDS, Table_parse_swaps_HELP},
	{"is_fs_mounted", (PyCFunction)Table_is_fs_mounted, METH_VARARGS|METH_KEYWORDS, Table_is_fs_mounted_HELP},
	{"add_fs", (PyCFunction)Table_add_fs, METH_VARARGS|METH_KEYWORDS, Table_add_fs_HELP},
	{"remove_fs", (PyCFunction)Table_remove_fs, METH_VARARGS|METH_KEYWORDS, Table_remove_fs_HELP},
	{"next_fs", (PyCFunction)Table_next_fs, METH_NOARGS, Table_next_fs_HELP},
	{"write_file", (PyCFunction)Table_write_file, METH_VARARGS|METH_KEYWORDS, Table_write_file_HELP},
	{"replace_file", (PyCFunction)Table_replace_file, METH_VARARGS|METH_KEYWORDS, Table_replace_file_HELP},
	{NULL}
};

/* mnt_free_tab() with a few necessary additions */
void Table_unref(struct libmnt_table *tab)
{
	struct libmnt_fs *fs;
	struct libmnt_iter *iter;

	if (!tab)
		return;

	DBG(TAB, pymnt_debug_h(tab, "un-referencing filesystems"));

	iter = mnt_new_iter(MNT_ITER_BACKWARD);

	/* remove pylibmount specific references to the entries */
	while (mnt_table_next_fs(tab, iter, &fs) == 0)
		Py_XDECREF(mnt_fs_get_userdata(fs));

	DBG(TAB, pymnt_debug_h(tab, "un-referencing table"));

	mnt_unref_table(tab);
	mnt_free_iter(iter);
}

static void Table_destructor(TableObject *self)
{
	DBG(TAB, pymnt_debug_h(self->tab, "destructor py-obj: %p, py-refcnt=%d",
				self, (int) ((PyObject *) self)->ob_refcnt));
	Table_unref(self->tab);
	self->tab = NULL;

	mnt_free_iter(self->iter);
	Py_XDECREF(self->errcb);
	PyFree(self);
}

static PyObject *Table_new(PyTypeObject *type,
			   PyObject *args __attribute__((unused)),
			   PyObject *kwds __attribute__((unused)))
{
	TableObject *self = (TableObject*)type->tp_alloc(type, 0);

	if (self) {
		DBG(TAB, pymnt_debug_h(self, "new"));

		self->tab = NULL;
		self->iter = NULL;
		self->errcb = NULL;
	}
	return (PyObject *)self;
}

/* explicit tab.__init__() serves as mnt_reset_table(tab) would in C
 * and as mnt_new_table{,_from_dir,_from_file}() with proper arguments */
#define Table_HELP "Table(path=None, errcb=None)"
static int Table_init(TableObject *self, PyObject *args, PyObject *kwds)
{
	struct libmnt_cache *cache;
	char *path = NULL;
	char *kwlist[] = {"path", "errcb", NULL};
	PyObject *errcb = NULL;
	struct stat buf;

	memset (&buf, 0, sizeof(struct stat));

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|sO",
					kwlist, &path, &errcb))
		return -1;

	DBG(TAB, pymnt_debug_h(self, "init"));

	Table_unref(self->tab);
	self->tab = NULL;

	if (self->iter)
		mnt_reset_iter(self->iter, MNT_ITER_FORWARD);
	else
		self->iter = mnt_new_iter(MNT_ITER_FORWARD);

	if (errcb) {
		PyObject *tmp;
		if (!PyCallable_Check(errcb))
			return -1;
		tmp = self->errcb;
		Py_INCREF(errcb);
		self->errcb = errcb;
		Py_XDECREF(tmp);
	} else {
		Py_XDECREF(self->errcb);
		self->errcb = NULL;
	}

	if (path) {
		DBG(TAB, pymnt_debug_h(self, "init: path defined (%s)", path));

		if (stat(path, &buf)) {
			/* TODO: weird */
			PyErr_SetFromErrno(PyExc_RuntimeError);
			return -1;
		}
		if (S_ISREG(buf.st_mode))
			self->tab = mnt_new_table_from_file(path);
		else if (S_ISDIR(buf.st_mode))
			self->tab = mnt_new_table_from_dir(path);
	} else {
		DBG(TAB, pymnt_debug_h(self, "init: allocate empty table"));
		self->tab = mnt_new_table();
	}

	/* Always set custom handler when using libmount from python */
	mnt_table_set_parser_errcb(self->tab, pymnt_table_parser_errcb);
	mnt_table_set_userdata(self->tab, self);

	cache = mnt_new_cache();		/* TODO: make it optional? */
	if (!cache)
		return -1;
	mnt_table_set_cache(self->tab, cache);
	mnt_unref_cache(cache);

	return 0;
}

/* Handler for the tab->errcb callback */
int pymnt_table_parser_errcb(struct libmnt_table *tb, const char *filename, int line)
{
	int rc = 0;
	PyObject *obj;

	obj = mnt_table_get_userdata(tb);
	if (obj && ((TableObject*) obj)->errcb) {
		PyObject *arglist, *result;

		arglist = Py_BuildValue("(Osi)", obj, filename, line);
		if (!arglist)
			return -ENOMEM;

		/* A python callback was set, so tb is definitely encapsulated in an object */
		result = PyObject_Call(((TableObject *)obj)->errcb, arglist, NULL);
		Py_DECREF(arglist);

		if (!result)
			return -EINVAL;
		if (!PyArg_Parse(result, "i", &rc))
			rc = -EINVAL;
		Py_DECREF(result);
	}
	return rc;
}

PyObject *PyObjectResultTab(struct libmnt_table *tab)
{
	TableObject *result;

	if (!tab) {
		PyErr_SetString(LibmountError, "internal exception");
		return NULL;
	}

	result = mnt_table_get_userdata(tab);
	if (result) {
		Py_INCREF(result);
		DBG(TAB, pymnt_debug_h(tab, "result py-obj %p: already exists, py-refcnt=%d",
				result, (int) ((PyObject *) result)->ob_refcnt));
		return (PyObject *) result;
	}

	/* Creating an encapsulating object: increment the refcount, so that
	 * code such as: cxt.get_fstab() doesn't call the destructor, which
	 * would free our tab struct as well
	 */
	result = PyObject_New(TableObject, &TableType);
	if (!result) {
		UL_RaiseExc(ENOMEM);
		return NULL;
	}

	Py_INCREF(result);
	mnt_ref_table(tab);

	DBG(TAB, pymnt_debug_h(tab, "result py-obj %p new, py-refcnt=%d",
				result, (int) ((PyObject *) result)->ob_refcnt));
	result->tab = tab;
	result->iter = mnt_new_iter(MNT_ITER_FORWARD);
	mnt_table_set_userdata(result->tab, result);
	result->errcb = NULL;
	return (PyObject *) result;
}

static PyGetSetDef Table_getseters[] = {
	{"nents",		(getter)Table_get_nents, NULL, "number of valid entries in tab", NULL},
	{"intro_comment",	(getter)Table_get_intro_comment, (setter)Table_set_intro_comment, "fstab intro comment", NULL},
	{"trailing_comment",	(getter)Table_get_trailing_comment, (setter)Table_set_trailing_comment, "fstab trailing comment", NULL},
	{"errcb",		NULL, (setter)Table_set_parser_errcb, "parser error callback", NULL},
	{NULL}
};


static PyObject *Table_repr(TableObject *self)
{
	return PyUnicode_FromFormat(
			"<libmount.Table object at %p, entries=%d, comments_enabled=%s, errcb=%s>",
			self,
			mnt_table_get_nents(self->tab),
			mnt_table_with_comments(self->tab) ? "True" : "False",
			self->errcb ? pystos(PyObject_Repr(self->errcb)) : "None");
}

PyTypeObject TableType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libmount.Table",
	.tp_basicsize = sizeof(TableObject),
	.tp_dealloc = (destructor)Table_destructor,
	.tp_repr = (reprfunc) Table_repr,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_doc = Table_HELP,
	.tp_methods = Table_methods,
	.tp_members = Table_members,
	.tp_getset = Table_getseters,
	.tp_init = (initproc)Table_init,
	.tp_new = Table_new,
};

void Table_AddModuleObject(PyObject *mod)
{
	if (PyType_Ready(&TableType) < 0)
		return;

	DBG(TAB, pymnt_debug("add to module"));

	Py_INCREF(&TableType);
	PyModule_AddObject(mod, "Table", (PyObject *)&TableType);
}

