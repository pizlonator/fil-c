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

static PyMemberDef Context_members[] = {
	{ NULL }
};

static PyObject *Context_set_tables_errcb(ContextObjext *self, PyObject *func,
				      void *closure __attribute__((unused)))
{
	if (!func) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return NULL;
	}

	if (!PyCallable_Check(func))
		return NULL;

	PyObject *tmp = self->table_errcb;
	Py_INCREF(func);
	self->table_errcb = func;
	Py_XDECREF(tmp);

	return UL_IncRef(self);
}

static void Context_dealloc(ContextObjext *self)
{
	if (!self->cxt) /* if init fails */
		return;

	Py_XDECREF(mnt_context_get_fs_userdata(self->cxt));
	Py_XDECREF(mnt_context_get_fstab_userdata(self->cxt));
	Py_XDECREF(mnt_context_get_mtab_userdata(self->cxt));

	mnt_free_context(self->cxt);
	PyFree(self);
}

static PyObject *Context_new(PyTypeObject *type,
			 PyObject *args __attribute__((unused)),
			 PyObject *kwds __attribute__((unused)))
{
	ContextObjext *self = (ContextObjext*) type->tp_alloc(type, 0);

	if (self) {
		self->cxt = NULL;
		self->table_errcb = NULL;
	}

	return (PyObject *)self;
}

/*
 * Note there is no pointer to encapsulating object needed here, since Cxt is
 * on top of the Context(Table(Filesystem)) hierarchy
 */
#define Context_HELP "Context(source=None, target=None, fstype=None, " \
		"options=None, mflags=0, fstype_pattern=None, " \
		"options_pattern=None, fs=None, fstab=None, optsmode=0)"
static int Context_init(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	char *source = NULL, *target = NULL, *fstype = NULL;
	char *options = NULL, *fstype_pattern = NULL, *options_pattern = NULL;
	unsigned long mflags = 0;
	int optsmode = 0, syscall_status = 1;
	FsObject *fs = NULL;
	TableObject *fstab = NULL;
	int rc = 0;
	char *kwlist[] = {
		"source", "target", "fstype",
		"options", "mflags", "fstype_pattern",
		"options_pattern", "fs", "fstab",
		"optsmode", NULL
	};

	if (!PyArg_ParseTupleAndKeywords(
				args, kwds, "|sssskssO!O!i", kwlist,
				&source, &target, &fstype, &options, &mflags,
				&fstype_pattern, &options_pattern, &FsType, &fs,
				&TableType, &fstab, &optsmode, &syscall_status)) {
			PyErr_SetString(PyExc_TypeError, ARG_ERR);
			return -1;
	}

	if (self->cxt)
		mnt_free_context(self->cxt);

	self->cxt = mnt_new_context();
	if (!self->cxt) {
		PyErr_SetString(PyExc_MemoryError, MEMORY_ERR);
		return -1;
	}

	if (source && (rc = mnt_context_set_source(self->cxt, source))) {
		UL_RaiseExc(-rc);
		return -1;
	}

	if (target && (rc = mnt_context_set_target(self->cxt, target))) {
		UL_RaiseExc(-rc);
		return -1;
	}

	if (fstype && (rc = mnt_context_set_fstype(self->cxt, fstype))) {
		UL_RaiseExc(-rc);
		return -1;
	}

	if (options && (rc = mnt_context_set_options(self->cxt, options))) {
		UL_RaiseExc(-rc);
		return -1;
	}

	if (fstype_pattern && (rc = mnt_context_set_fstype_pattern(self->cxt, fstype_pattern))) {
		UL_RaiseExc(-rc);
		return -1;
	}

	if (options_pattern && (rc = mnt_context_set_options_pattern(self->cxt, options_pattern))) {
		UL_RaiseExc(-rc);
		return -1;
	}

	if (fs && (rc = mnt_context_set_fs(self->cxt, fs->fs))) {
		UL_RaiseExc(-rc);
		return -1;
	}

	if (fstab && (rc = mnt_context_set_fstab(self->cxt, fstab->tab))) {
		UL_RaiseExc(-rc);
		return -1;
	}

	if (optsmode && (rc = mnt_context_set_optsmode(self->cxt, optsmode))) {
		UL_RaiseExc(-rc);
		return -1;
	}

	mnt_context_set_mflags(self->cxt, mflags);
	mnt_context_set_optsmode(self->cxt, optsmode);
	mnt_context_set_tables_errcb(self->cxt, pymnt_table_parser_errcb);

	return 0;
}

#define Context_enable_fake_HELP "enable_fake(enable)\n\n" \
	"Enable/disable fake mounting (see mount(8) man page, option -f).\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_enable_fake(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	int enable;
	char *kwlist[] = { "enable", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &enable)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_context_enable_fake(self->cxt, enable);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_enable_force_HELP "enable_force(enable)\n\n" \
	"Enable/disable force umounting (see umount(8) man page, option -f).\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_enable_force(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	int enable;
	char *kwlist[] = { "enable", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &enable)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_context_enable_force(self->cxt, enable);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_enable_lazy_HELP "enable_lazy(enable)\n\n" \
	"Enable/disable lazy umount (see umount(8) man page, option -l).\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_enable_lazy(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	int enable;
	char *kwlist[] = { "enable", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &enable)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_context_enable_lazy(self->cxt, enable);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_enable_loopdel_HELP "enable_loopdel(enable)\n\n" \
	"Enable/disable loop delete (destroy) after umount (see umount(8), option -d)\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_enable_loopdel(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	int enable;
	char *kwlist[] = { "enable", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &enable)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_context_enable_loopdel(self->cxt, enable);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_enable_rdonly_umount_HELP "enable_rdonly_umount(enable)\n\n" \
	"Enable/disable read-only remount on failed umount(2)\n "\
	"(see umount(8) man page, option -r).\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_enable_rdonly_umount(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	int enable;
	char *kwlist[] = { "enable", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &enable)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_context_enable_rdonly_umount(self->cxt, enable);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_enable_sloppy_HELP "enable_sloppy(enable)\n\n" \
	"Set/unset sloppy mounting (see mount(8) man page, option -s).\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_enable_sloppy(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	int enable;
	char *kwlist[] = { "enable", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &enable)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_context_enable_sloppy(self->cxt, enable);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_enable_verbose_HELP "enable_verbose(enable)\n\n" \
	"Enable/disable verbose output (TODO: not implemented yet)\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_enable_verbose(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	int enable;
	char *kwlist[] = { "enable", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &enable)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_context_enable_verbose(self->cxt, enable);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_enable_fork_HELP "enable_fork(enable)\n\n" \
	"Enable/disable fork(2) call in Cxt.next_mount()(not yet implemented) (see mount(8) man\n" \
	"page, option -F).\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_enable_fork(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	int enable;
	char *kwlist[] = {"enable", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &enable)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_context_enable_fork(self->cxt, enable);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_disable_canonicalize_HELP "disable_canonicalize(disable)\n\n" \
	"Enable/disable paths canonicalization and tags evaluation. The libmount context\n" \
	"canonicalizes paths when searching fstab and when preparing source and target paths\n" \
	"for mount(2) syscall.\n" \
	"\n" \
	"This function has effect to the private (within context) fstab instance only\n" \
	"(see Cxt.fstab).\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_disable_canonicalize(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	int disable;
	char *kwlist[] = {"disable", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &disable)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_context_disable_canonicalize(self->cxt, disable);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_disable_helpers_HELP "disable_helpers(disable)\n\n" \
	"Enable/disable /sbin/[u]mount.* helpers (see mount(8) man page, option -i).\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_disable_helpers(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	int disable;
	char *kwlist[] = {"disable", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &disable)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_context_disable_helpers(self->cxt, disable);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_disable_mtab_HELP "disable_mtab(disable)\n\n" \
	"Disable/enable mtab update (see mount(8) man page, option -n).\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_disable_mtab(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	int disable;
	char *kwlist[] = {"disable", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &disable)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_context_disable_mtab(self->cxt, disable);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_disable_swapmatch_HELP "disable_swapmatch(disable)\n\n" \
	"Disable/enable swap between source and target for mount(8) if only one path\n" \
	"is specified.\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_disable_swapmatch(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	int disable;
	char *kwlist[] = { "disable", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &disable)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	rc = mnt_context_disable_swapmatch(self->cxt, disable);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

static int Context_set_source(ContextObjext *self, PyObject *value, void *closure __attribute__((unused)))
{
	char *source;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(source = pystos(value)))
		return -1;

	rc = mnt_context_set_source(self->cxt, source);
	if (rc) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}

static int Context_set_mountdata(ContextObjext *self, PyObject *value, void *closure __attribute__((unused)))
{
	char *mountdata;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(mountdata = pystos(value)))
		return -1;

	rc = mnt_context_set_mountdata(self->cxt, mountdata);
	if (rc) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}

static int Context_set_target(ContextObjext *self, PyObject *value, void *closure __attribute__((unused)))
{
	char * target;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(target = pystos(value)))
		return -1;

	rc = mnt_context_set_target(self->cxt, target);
	if (rc) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}

static int Context_set_fstype(ContextObjext *self, PyObject *value, void *closure __attribute__((unused)))
{
	char * fstype;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(fstype = pystos(value)))
		return -1;

	rc = mnt_context_set_fstype(self->cxt, fstype);
	if (rc) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}

static int Context_set_options(ContextObjext *self, PyObject *value, void *closure __attribute__((unused)))
{
	char * options;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(options = pystos(value)))
		return -1;

	rc = mnt_context_set_options(self->cxt, options);
	if (rc) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}

static int Context_set_fstype_pattern(ContextObjext *self, PyObject *value, void *closure __attribute__((unused)))
{
	char * fstype_pattern;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(fstype_pattern = pystos(value)))
		return -1;

	rc = mnt_context_set_fstype_pattern(self->cxt, fstype_pattern);
	if (rc) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}

static int Context_set_options_pattern(ContextObjext *self, PyObject *value, void *closure __attribute__((unused)))
{
	char * options_pattern;
	int rc = 0;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!(options_pattern = pystos(value)))
		return -1;

	rc = mnt_context_set_options_pattern(self->cxt, options_pattern);
	if (rc) {
		UL_RaiseExc(-rc);
		return -1;
	}
	return 0;
}

static int Context_set_fs(ContextObjext *self, PyObject *value, void *closure __attribute__((unused)))
{
	FsObject *fs;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!PyArg_Parse(value, "O!", &FsType, &fs)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return -1;
	}
	Py_INCREF(fs);
	Py_XDECREF(mnt_context_get_fs_userdata(self->cxt));

	return mnt_context_set_fs(self->cxt, fs->fs);
}

static int Context_set_fstab(ContextObjext *self, PyObject *value, void *closure __attribute__((unused)))
{
	TableObject *fstab;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!PyArg_Parse(value, "O!", &TableType, &fstab)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return -1;
	}
	Py_INCREF(fstab);
	Py_XDECREF(mnt_context_get_fstab_userdata(self->cxt));

	return mnt_context_set_fstab(self->cxt, fstab->tab);
}

static int Context_set_optsmode(ContextObjext *self, PyObject *value, void *closure __attribute__((unused)))
{
	int optsmode;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!PyLong_Check(value)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return -1;
	}
	optsmode = PyLong_AsLong(value);
	return mnt_context_set_optsmode(self->cxt, optsmode);
}

static int Context_set_syscall_status(ContextObjext *self, PyObject *value, void *closure __attribute__((unused)))
{
	int syscall_status;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!PyLong_Check(value)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return -1;
	}
	syscall_status = PyLong_AsLong(value);
	return mnt_context_set_syscall_status(self->cxt, syscall_status);
}

static int Context_set_user_mflags(ContextObjext *self, PyObject *value, void *closure __attribute__((unused)))
{
	unsigned long flags;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!PyLong_Check(value)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return -1;
	}
	flags = PyLong_AsUnsignedLong(value);
	return mnt_context_set_mflags(self->cxt, flags);

}

static int Context_set_mflags(ContextObjext *self, PyObject *value, void *closure __attribute__((unused)))
{
	unsigned long flags;

	if (!value) {
		PyErr_SetString(PyExc_TypeError, NODEL_ATTR);
		return -1;
	}
	if (!PyLong_Check(value)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return -1;
	}
	flags = PyLong_AsUnsignedLong(value);
	return mnt_context_set_mflags(self->cxt, flags);
}

/* returns a flags integer (behavior differs from C API) */
static PyObject *Context_get_mflags(ContextObjext *self)
{
	unsigned long flags;

	PyObject *result;
	mnt_context_get_mflags(self->cxt, &flags);
	result = Py_BuildValue("k", flags);

	if (!result)
		PyErr_SetString(PyExc_RuntimeError, CONSTRUCT_ERR);
	return result;

}
/* returns a flags integer (behavior differs from C API) */
static PyObject *Context_get_user_mflags(ContextObjext *self)
{
	unsigned long flags;

	PyObject *result;
	mnt_context_get_user_mflags(self->cxt, &flags);
	result = Py_BuildValue("k", flags);

	if (!result)
		PyErr_SetString(PyExc_RuntimeError, CONSTRUCT_ERR);
	return result;

}
#define Context_reset_status_HELP "reset_status()\n\n" \
	"Resets mount(2) and mount.type statuses, so Cxt.do_mount() or\n" \
	"Cxt.do_umount() could be again called with the same settings.\n" \
	"\n" \
	"BE CAREFUL -- after this soft reset the libmount will NOT parse mount\n" \
	"options, evaluate permissions or apply stuff from fstab.\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_reset_status(ContextObjext *self)
{
	int rc = mnt_context_reset_status(self->cxt);

	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_is_fake_HELP "is_fake()\n\n" \
"Returns True if fake flag is enabled or False"
static PyObject *Context_is_fake(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_is_fake(self->cxt));
}

#define Context_is_force_HELP "is_force()\n\n" \
"Returns True if force umounting flag is enabled or False"
static PyObject *Context_is_force(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_is_force(self->cxt));
}

#define Context_is_lazy_HELP "is_lazy()\n\n" \
"Returns True if lazy umount is enabled or False"
static PyObject *Context_is_lazy(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_is_lazy(self->cxt));
}

#define Context_is_nomtab_HELP "is_nomtab()\n\n" \
	"Returns True if no-mtab is enabled or False"
static PyObject *Context_is_nomtab(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_is_nomtab(self->cxt));
}

#define Context_is_rdonly_umount_HELP "is_rdonly_umount()\n\n" \
	"Enable/disable read-only remount on failed umount(2)\n" \
	"(see umount(8) man page, option -r).\n" \
	"\n" \
	"Returns self on success, raises an exception in case of error."
static PyObject *Context_is_rdonly_umount(ContextObjext *self)
{
	int rc = mnt_context_is_rdonly_umount(self->cxt);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_is_restricted_HELP "is_restricted()\n\n" \
	"Returns False for unrestricted mount (user is root), or True for non-root mounts"
static PyObject *Context_is_restricted(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_is_restricted(self->cxt));
}

#define Context_is_sloppy_HELP "is_sloppy()\n\n" \
	"Returns True if sloppy flag is enabled or False"
static PyObject *Context_is_sloppy(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_is_sloppy(self->cxt));
}

#define Context_is_verbose_HELP "is_verbose()\n\n" \
	"Returns True if verbose flag is enabled or False"
static PyObject *Context_is_verbose(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_is_verbose(self->cxt));
}
#define Context_is_fs_mounted_HELP "is_fs_mounted(fs, mounted)\n\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_is_fs_mounted(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"fs", "mounted", NULL};
	FsObject *fs;
	int mounted;

	if (PyArg_ParseTupleAndKeywords(args, kwds, "O!i", kwlist,
					&FsType, &fs, &mounted)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	return PyBool_FromLong(mnt_context_is_fs_mounted(self->cxt, fs->fs, &mounted));
}

#define Context_is_child_HELP "is_child()\n\n" \
	"Returns True if mount -F enabled and the current context is child, or False"
static PyObject *Context_is_child(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_is_child(self->cxt));
}

#define Context_is_fork_HELP "is_fork()\n\n" \
	"Returns True if fork (mount -F) is enabled or False"
static PyObject *Context_is_fork(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_is_fork(self->cxt));
}

#define Context_is_parent_HELP "is_parent()\n\n" \
	"Returns True if mount -F enabled and the current context is parent, or False"
static PyObject *Context_is_parent(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_is_parent(self->cxt));
}

#define Context_is_loopdel_HELP "is_loopdel()\n\n" \
	"Returns True if loop device should be deleted after umount (umount -d) or False."
static PyObject *Context_is_loopdel(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_is_loopdel(self->cxt));
}

#define Context_is_nocanonicalize_HELP "is_nocanonicalize()\n\n" \
	"Returns True if no-canonicalize mode enabled or False."
static PyObject *Context_is_nocanonicalize(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_is_nocanonicalize(self->cxt));
}

#define Context_is_nohelpers_HELP "is_nohelpers()\n\n" \
	"Returns True if helpers are disabled (mount -i) or False."
static PyObject *Context_is_nohelpers(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_is_nohelpers(self->cxt));
}

#define Context_syscall_called_HELP "syscall_called()\n\n" \
	"Returns True if mount(2) syscall has been called, or False."
static PyObject *Context_syscall_called(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_syscall_called(self->cxt));
}

#define Context_is_swapmatch_HELP "is_swapmatch()\n\n" \
	"Returns True if swap between source and target is allowed (default is True) or False."
static PyObject *Context_is_swapmatch(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_is_swapmatch(self->cxt));
}

#define Context_tab_applied_HELP "tab_applied()\n\n" \
	"Returns True if fstab (or mtab) has been applied to the context, False otherwise."
static PyObject *Context_tab_applied(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_tab_applied(self->cxt));
}

#define Context_apply_fstab_HELP "apply_fstab()\n\n" \
	"This function is optional.\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_apply_fstab(ContextObjext *self)
{
	int rc = mnt_context_apply_fstab(self->cxt);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_helper_executed_HELP "helper_executed()\n\n" \
	"Returns True if mount.type helper has been executed, or False."
static PyObject *Context_helper_executed(ContextObjext *self)
{
	return PyBool_FromLong(mnt_context_helper_executed(self->cxt));
}

static PyObject *Context_get_source(ContextObjext *self)
{
	return PyObjectResultStr(mnt_context_get_source(self->cxt));
}

static PyObject *Context_get_target(ContextObjext *self)
{
	return PyObjectResultStr(mnt_context_get_target(self->cxt));
}

static PyObject *Context_get_options(ContextObjext *self)
{
	return PyObjectResultStr(mnt_context_get_options(self->cxt));
}

static PyObject *Context_get_fstype(ContextObjext *self)
{
	return PyObjectResultStr(mnt_context_get_fstype(self->cxt));
}

static PyObject *Context_get_fs(ContextObjext *self)
{
	return PyObjectResultFs(mnt_context_get_fs(self->cxt));
}

static PyObject *Context_get_fstab(ContextObjext *self)
{
	struct libmnt_table *tab = NULL;

	if (mnt_context_get_fstab(self->cxt, &tab) != 0 || !tab)
		return NULL;
	return PyObjectResultTab(tab);
}

static PyObject *Context_get_mtab(ContextObjext *self)
{
	struct libmnt_table *tab = NULL;

	if (mnt_context_get_mtab(self->cxt, &tab) != 0 || !tab)
		return NULL;
	return PyObjectResultTab(tab);
}

static PyObject *Context_get_optsmode(ContextObjext *self)
{
	return PyObjectResultInt(mnt_context_get_optsmode(self->cxt));
}

static PyObject *Context_get_status(ContextObjext *self)
{
	return PyObjectResultInt(mnt_context_get_status(self->cxt));
}

static PyObject *Context_get_syscall_errno(ContextObjext *self)
{
	return PyObjectResultInt(mnt_context_get_syscall_errno(self->cxt));
}

#define Context_do_mount_HELP "do_mount()\n\n" \
	"Call mount(2) or mount.type helper. Unnecessary for Cxt.mount().\n" \
	"\n" \
	"Note that this function could be called only once. If you want to mount\n" \
	"another source or target than you have to call Cxt.reset_context().\n" \
	"\n" \
	"If you want to call mount(2) for the same source and target with a different\n" \
	"mount flags or fstype then call Cxt.reset_status() and then try\n" \
	"again Cxt.do_mount().\n" \
	"\n" \
	"WARNING: non-zero return code does not mean that mount(2) syscall or\n" \
	"mount.type helper wasn't successfully called.\n" \
	"\n" \
	"Check Cxt.status after error!\n" \
	"\n" \
	"Returns self on success or an exception in case of other errors."
static PyObject *Context_do_mount(ContextObjext *self)
{
	int rc = mnt_context_do_mount(self->cxt);
	return rc ? UL_RaiseExc(rc < 0 ? -rc : rc) : UL_IncRef(self);
}

#define Context_do_umount_HELP "do_umount()\n\n" \
	"Umount filesystem by umount(2) or fork()+exec(/sbin/umount.type).\n" \
	"Unnecessary for Cxt.umount().\n" \
	"\n" \
	"See also Cxt.disable_helpers().\n" \
	"\n" \
	"WARNING: non-zero return code does not mean that umount(2) syscall or\n" \
	"umount.type helper wasn't successfully called.\n" \
	"\n" \
	"Check Cxt.status after error!\n" \
	"\n" \
	"Returns self on success or an exception in case of other errors."
static PyObject *Context_do_umount(ContextObjext *self)
{
	int rc = mnt_context_do_umount(self->cxt);
	return rc ? UL_RaiseExc(rc < 0 ? -rc : rc) : UL_IncRef(self);
}

#define Context_mount_HELP "mount()\n\n" \
	"High-level, mounts filesystem by mount(2) or fork()+exec(/sbin/mount.type).\n" \
	"\n" \
	"This is similar to:\n" \
	"\n" \
	"Cxt.prepare_mount();\n" \
	"Cxt.do_mount();\n" \
	"Cxt.finalize_mount();\n" \
	"\n" \
	"See also Cxt.disable_helper().\n" \
	"\n" \
	"Note that this function could be called only once. If you want to mount with\n" \
	"different setting than you have to call Cxt.reset_context(). It's NOT enough\n" \
	"to call Cxt.reset_status() if you want call this function more than\n" \
	"once, whole context has to be reset.\n" \
	"\n" \
	"WARNING: non-zero return code does not mean that mount(2) syscall or\n" \
	"mount.type helper wasn't successfully called.\n" \
	"\n" \
	"Check Cxt.status after error!\n" \
	"\n" \
	"Returns self on success or an exception in case of other errors."
static PyObject *Context_mount(ContextObjext *self)
{
	int rc = mnt_context_mount(self->cxt);
	return rc ? UL_RaiseExc(rc < 0 ? -rc : rc) : UL_IncRef(self);
}

#define Context_umount_HELP "umount()\n\n" \
	"High-level, umounts filesystem by umount(2) or fork()+exec(/sbin/umount.type).\n" \
	"\n" \
	"This is similar to:\n" \
	"\n" \
	"Cxt.prepare_umount();\n" \
	"Cxt.do_umount();\n" \
	"Cxt.finalize_umount();\n" \
	"\n" \
	"See also Cxt.disable_helpers().\n" \
	"\n" \
	"WARNING: non-zero return code does not mean that umount(2) syscall or\n" \
	"umount.type helper wasn't successfully called.\n" \
	"\n" \
	"Check Cxt.status after error!\n" \
	"\n" \
	"Returns self on success or an exception in case of other errors."
static PyObject *Context_umount(ContextObjext *self)
{
	int rc = mnt_context_umount(self->cxt);
	return rc ? UL_RaiseExc(rc < 0 ? -rc : rc) : UL_IncRef(self);
}

#define Context_finalize_mount_HELP "finalize_mount()\n\n" \
	"Mtab update, etc. Unnecessary for Cxt.mount(), but should be called\n" \
	"after Cxt.do_mount(). See also Cxt.syscall_status.\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_finalize_mount(ContextObjext *self)
{
	int rc = mnt_context_finalize_mount(self->cxt);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_prepare_umount_HELP "prepare_umount()\n\n" \
	"Prepare context for umounting, unnecessary for Cxt.umount().\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_prepare_umount(ContextObjext *self)
{
	int rc = mnt_context_prepare_umount(self->cxt);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_prepare_mount_HELP "prepare_mount()\n\n" \
	"Prepare context for mounting, unnecessary for Cxt.mount().\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_prepare_mount(ContextObjext *self)
{
	int rc = mnt_context_prepare_mount(self->cxt);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_finalize_umount_HELP "finalize_umount()\n\n" \
	"Mtab update, etc. Unnecessary for Cxt.umount(), but should be called\n" \
	"after Cxt.do_umount(). See also Cxt.syscall_status.\n" \
	"\n" \
	"Returns self on success, raises LibmountError if target filesystem not found, or other exception on error."
static PyObject *Context_finalize_umount(ContextObjext *self)
{
	int rc = mnt_context_finalize_umount(self->cxt);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_find_umount_fs_HELP "find_umount_fs(tgt, pfs)\n\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_find_umount_fs(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	char *kwlist[] = { "tgt", "pfs", NULL };
	char *tgt = NULL;
	FsObject *fs;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "sO!", kwlist, &tgt, &FsType, &fs)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}

	rc = mnt_context_find_umount_fs(self->cxt, tgt, &fs->fs);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_append_options_HELP "append_options(optstr)\n\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_append_options(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	char *kwlist[] = {"optstr", NULL};
	char *optstr = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &optstr)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}

	rc = mnt_context_append_options(self->cxt, optstr);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_helper_setopt_HELP "helper_setopt(c, arg)\n\n" \
	"This function applies [u]mount.type command line option (for example parsed\n" \
	"by getopt or getopt_long) to cxt. All unknown options are ignored and\n" \
	"then ValueError is raised.\n" \
	"\n" \
	"Returns self on success, raises ValueError if c is unknown or other exception in case of an error."
static PyObject *Context_helper_setopt(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	int c;
	char *arg;
	char *kwlist[] = { "c", "arg", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "is", kwlist, &c, &arg)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}

	rc = mnt_context_helper_setopt(self->cxt, c, arg);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

#define Context_init_helper_HELP "init_helper(action, flags)\n\n" \
	"This function informs libmount that it is used from [u]mount.type helper.\n" \
	"\n" \
	"The function also calls Cxt.disable_helpers() to avoid calling\n" \
	"mount.type helpers recursively. If you really want to call another\n" \
	"mount.type helper from your helper then you have to explicitly enable this\n" \
	"feature by:\n" \
	"\n" \
	"Cxt.disable_helpers(False);\n" \
	"\n" \
	"Returns self or raises an exception in case of an error."
static PyObject *Context_init_helper(ContextObjext *self, PyObject *args, PyObject *kwds)
{
	int rc;
	int action, flags;
	char *kwlist[] = {"action", "flags", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ii", kwlist, &action, &flags)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}

	rc = mnt_context_init_helper(self->cxt, action, flags);
	return rc ? UL_RaiseExc(-rc) : UL_IncRef(self);
}

static PyGetSetDef Context_getseters[] = {
	{"tables_errcb",	NULL, (setter)Context_set_tables_errcb, "error callback function", NULL},
	{"status",		(getter)Context_get_status, NULL, "status", NULL},
	{"source",		(getter)Context_get_source, (setter)Context_set_source, "source", NULL},
	{"target",		(getter)Context_get_target, (setter)Context_set_target, "target", NULL},
	{"fstype",		(getter)Context_get_fstype, (setter)Context_set_fstype, "fstype", NULL},
	{"options",		(getter)Context_get_options, (setter)Context_set_options, "options", NULL},
	{"mflags",		(getter)Context_get_mflags, (setter)Context_set_mflags, "mflags", NULL},
	{"mountdata",		NULL, (setter)Context_set_mountdata, "mountdata", NULL},
	{"fstype_pattern",	NULL, (setter)Context_set_fstype_pattern, "fstype_pattern", NULL},
	{"options_pattern",	NULL, (setter)Context_set_options_pattern, "options_pattern", NULL},
	{"fs",			(getter)Context_get_fs, (setter)Context_set_fs, "filesystem description (type, mountpoint, device, ...)", NULL},
	{"mtab",		(getter)Context_get_mtab, NULL, "mtab entries", NULL},
	{"fstab",		(getter)Context_get_fstab, (setter)Context_set_fstab, "fstab (or mtab for some remounts)", NULL},
	{"optsmode",		(getter)Context_get_optsmode, (setter)Context_set_optsmode, "fstab optstr mode MNT_OPTSMODE_{AUTO,FORCE,IGNORE}", NULL},
	{"syscall_errno",	(getter)Context_get_syscall_errno, (setter)Context_set_syscall_status, "1: not_called yet, 0: success, <0: -errno", NULL},
	{"user_mflags",		(getter)Context_get_user_mflags, (setter)Context_set_user_mflags, "user mflags", NULL},
	{NULL}
};
static PyMethodDef Context_methods[] = {
	{"find_umount_fs",	(PyCFunction)Context_find_umount_fs, METH_VARARGS|METH_KEYWORDS, Context_find_umount_fs_HELP},
	{"reset_status",	(PyCFunction)Context_reset_status, METH_NOARGS, Context_reset_status_HELP},
	{"helper_executed",	(PyCFunction)Context_helper_executed, METH_NOARGS, Context_helper_executed_HELP},
	{"init_helper",	(PyCFunction)Context_init_helper, METH_VARARGS|METH_KEYWORDS, Context_init_helper_HELP},
	{"helper_setopt",	(PyCFunction)Context_helper_setopt, METH_VARARGS|METH_KEYWORDS, Context_helper_setopt_HELP},
	{"append_options",	(PyCFunction)Context_append_options, METH_VARARGS|METH_KEYWORDS, Context_append_options_HELP},
	{"apply_fstab",	(PyCFunction)Context_apply_fstab, METH_NOARGS, Context_apply_fstab_HELP},
	{"disable_canonicalize",	(PyCFunction)Context_disable_canonicalize, METH_VARARGS|METH_KEYWORDS, Context_disable_canonicalize_HELP},
	{"disable_helpers",	(PyCFunction)Context_disable_helpers, METH_VARARGS|METH_KEYWORDS, Context_disable_helpers_HELP},
	{"disable_mtab",	(PyCFunction)Context_disable_mtab, METH_VARARGS|METH_KEYWORDS, Context_disable_mtab_HELP},
	{"do_mount",	(PyCFunction)Context_do_mount, METH_NOARGS, Context_do_mount_HELP},
	{"do_umount",	(PyCFunction)Context_do_umount, METH_NOARGS , Context_do_umount_HELP},
	{"enable_fake",	(PyCFunction)Context_enable_fake, METH_VARARGS|METH_KEYWORDS, Context_enable_fake_HELP},
	{"enable_force",	(PyCFunction)Context_enable_force, METH_VARARGS|METH_KEYWORDS, Context_enable_force_HELP},
	{"enable_lazy",	(PyCFunction)Context_enable_lazy, METH_VARARGS|METH_KEYWORDS, Context_enable_lazy_HELP},
	{"enable_loopdel",	(PyCFunction)Context_enable_loopdel, METH_VARARGS|METH_KEYWORDS, Context_enable_loopdel_HELP},
	{"enable_rdonly_umount",	(PyCFunction)Context_enable_rdonly_umount, METH_VARARGS|METH_KEYWORDS, Context_enable_rdonly_umount_HELP},
	{"enable_sloppy",	(PyCFunction)Context_enable_sloppy, METH_VARARGS|METH_KEYWORDS, Context_enable_sloppy_HELP},
	{"enable_verbose",	(PyCFunction)Context_enable_verbose, METH_VARARGS|METH_KEYWORDS, Context_enable_verbose_HELP},
	{"enable_fork",	(PyCFunction)Context_enable_fork, METH_VARARGS|METH_KEYWORDS, Context_enable_fork_HELP},
	{"finalize_mount",	(PyCFunction)Context_finalize_mount, METH_NOARGS, Context_finalize_mount_HELP},
	{"finalize_umount",	(PyCFunction)Context_finalize_umount, METH_NOARGS, Context_finalize_umount_HELP},
	{"is_fake",	(PyCFunction)Context_is_fake, METH_NOARGS, Context_is_fake_HELP},
	{"is_force",	(PyCFunction)Context_is_force, METH_NOARGS, Context_is_force_HELP},
	{"is_fork",	(PyCFunction)Context_is_fork, METH_NOARGS, Context_is_fork_HELP},
	{"is_fs_mounted",	(PyCFunction)Context_is_fs_mounted, METH_VARARGS|METH_KEYWORDS, Context_is_fs_mounted_HELP},
	{"is_lazy",	(PyCFunction)Context_is_lazy, METH_NOARGS, Context_is_lazy_HELP},
	{"is_nomtab",	(PyCFunction)Context_is_nomtab, METH_NOARGS, Context_is_nomtab_HELP},
	{"is_rdonly_umount",	(PyCFunction)Context_is_rdonly_umount, METH_NOARGS, Context_is_rdonly_umount_HELP},
	{"is_restricted",	(PyCFunction)Context_is_restricted, METH_NOARGS, Context_is_restricted_HELP},
	{"is_sloppy",	(PyCFunction)Context_is_sloppy, METH_NOARGS, Context_is_sloppy_HELP},
	{"is_verbose",	(PyCFunction)Context_is_verbose, METH_NOARGS, Context_is_verbose_HELP},
	{"is_child",	(PyCFunction)Context_is_child, METH_NOARGS, Context_is_child_HELP},
	{"is_parent",	(PyCFunction)Context_is_parent, METH_NOARGS, Context_is_parent_HELP},
	{"is_loopdel",	(PyCFunction)Context_is_loopdel, METH_NOARGS, Context_is_loopdel_HELP},
	{"is_nocanonicalize",	(PyCFunction)Context_is_nocanonicalize, METH_NOARGS, Context_is_nocanonicalize_HELP},
	{"is_nohelpers",	(PyCFunction)Context_is_nohelpers, METH_NOARGS, Context_is_nohelpers_HELP},
	{"is_swapmatch",	(PyCFunction)Context_is_swapmatch, METH_NOARGS, Context_is_swapmatch_HELP},
	{"mount",	(PyCFunction)Context_mount, METH_NOARGS, Context_mount_HELP},
	{"prepare_mount",	(PyCFunction)Context_prepare_mount, METH_NOARGS, Context_prepare_mount_HELP},
	{"prepare_umount",	(PyCFunction)Context_prepare_umount, METH_NOARGS, Context_prepare_umount_HELP},
	{"umount",	(PyCFunction)Context_umount, METH_NOARGS, Context_umount_HELP},
	{"syscall_called",	(PyCFunction)Context_syscall_called, METH_NOARGS, Context_syscall_called_HELP},
	{"disable_swapmatch",	(PyCFunction)Context_disable_swapmatch, METH_VARARGS|METH_KEYWORDS, Context_disable_swapmatch_HELP},
	{"tab_applied",	(PyCFunction)Context_tab_applied, METH_NOARGS, Context_tab_applied_HELP},
	{NULL}
};

static PyObject *Context_repr(ContextObjext *self)
{
	return PyUnicode_FromFormat("<libmount.Context object at %p, restricted=%s>",
			self, mnt_context_is_restricted(self->cxt) ? "True" : "False");
}

PyTypeObject ContextType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libmount.Context",
	.tp_basicsize = sizeof(ContextObjext),
	.tp_dealloc = (destructor)Context_dealloc,
	.tp_repr = (reprfunc) Context_repr,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_doc = Context_HELP,
	.tp_methods = Context_methods,
	.tp_members = Context_members,
	.tp_getset = Context_getseters,
	.tp_init = (initproc)Context_init,
	.tp_new = Context_new,
};

void Context_AddModuleObject(PyObject *mod)
{
	if (PyType_Ready(&ContextType) < 0)
		return;

	DBG(CXT, pymnt_debug("add to module"));

	Py_INCREF(&ContextType);
	PyModule_AddObject(mod, "Context", (PyObject *)&ContextType);
}


