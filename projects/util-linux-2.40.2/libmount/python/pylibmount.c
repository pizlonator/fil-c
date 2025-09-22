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

/* Libmount-specific Exception class */
PyObject *LibmountError;
int pylibmount_debug_mask;

PyObject *UL_IncRef(void *killme)
{
	Py_INCREF(killme);
	return killme;
}

void PyFree(void *o)
{
#if PY_MAJOR_VERSION >= 3
	Py_TYPE(o)->tp_free((PyObject *)o);
#else
	((PyObject *)o)->ob_type->tp_free((PyObject *)o);
#endif
}

/* Demultiplexer for various possible error conditions across the libmount library */
void *UL_RaiseExc(int e)
{
	/* TODO: Do we need to deal with -1/1? */
	switch (e) {
		case ENOMEM:
			PyErr_SetString(PyExc_MemoryError, strerror(e));
			break;
		case EINVAL:
			PyErr_SetString(PyExc_TypeError, strerror(e));
			break;
		/* libmount-specific errors */
		case MNT_ERR_NOFSTAB:
			PyErr_SetString(LibmountError, "Not found required entry in fstab");
			break;
		case MNT_ERR_NOFSTYPE:
			PyErr_SetString(LibmountError, "Lailed to detect filesystem type");
			break;
		case MNT_ERR_NOSOURCE:
			PyErr_SetString(LibmountError, "Required mount source undefined");
			break;
		case MNT_ERR_LOOPDEV:
			PyErr_SetString(LibmountError, "Loopdev setup failed");
			break;
		case MNT_ERR_APPLYFLAGS:
			PyErr_SetString(LibmountError, "Failed to parse/use userspace mount options");
			break;
		case MNT_ERR_MOUNTOPT:
			PyErr_SetString(LibmountError, "Failed to apply propagation flags");
			break;
		case MNT_ERR_AMBIFS:
			PyErr_SetString(LibmountError, "Libblkid detected more filesystems on the device");
			break;
		case MNT_ERR_LOOPOVERLAP:
			PyErr_SetString(LibmountError, "Detected overlapping loop device that cannot be re-use");
			break;
		case MNT_ERR_LOCK:
			PyErr_SetString(LibmountError, "Failed to lock mtab/utab or so");
			break;
		case MNT_ERR_NAMESPACE:
			PyErr_SetString(LibmountError, "Failed to switch namespace");
			break;
		/* some other errno */
		default:
			PyErr_SetString(PyExc_Exception, strerror(e));
			break;
	}
	return NULL;
}

/*
 * General functions
 */
PyObject *PyObjectResultInt(int i)
{
	PyObject *result = Py_BuildValue("i", i);
	if (!result)
		PyErr_SetString(PyExc_RuntimeError, CONSTRUCT_ERR);
	return result;
}

PyObject *PyObjectResultStr(const char *s)
{
	PyObject *result;
	if (!s)
		/* TODO: maybe lie about it and return "":
		 * which would allow for
		 * fs = libmount.Fs()
		 * fs.comment += "comment"
		return Py_BuildValue("s", ""); */
		Py_RETURN_NONE;
	result = Py_BuildValue("s", s);
	if (!result)
		PyErr_SetString(PyExc_RuntimeError, CONSTRUCT_ERR);
	return result;
}

/* wrapper around a common use case for PyUnicode_AsASCIIString() */
char *pystos(PyObject *pys)
{
#if PY_MAJOR_VERSION >= 3
	if (!PyUnicode_Check(pys)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	return (char *)PyUnicode_1BYTE_DATA(pys);
#else
	if (!PyString_Check(pys)) {
		PyErr_SetString(PyExc_TypeError, ARG_ERR);
		return NULL;
	}
	return PyString_AsString(pys);
#endif
}

/*
 * the libmount module
 */
#define PYLIBMOUNT_DESC \
	"Python API for the util-linux libmount library.\n\n" \
	"Please note that none of the classes' attributes may be deleted.\n" \
	"This is not a complete mapping to the libmount C API, nor is it\n" \
	"attempting to be one.\n" "Iterator functions only allow forward\n" \
	"iteration for now. Context.get_{user_,}mflags() differs from the C API\n" \
	"and returns the flags directly. Fs.get_tag() differs from the C API\n" \
	"and returns a (tag, value) tuple. Every attribute is \"filtered\"" \
	"through appropriate getters/setters, no values are set directly."


struct module_state {
    PyObject *error;
};

#if PY_MAJOR_VERSION >= 3
#define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))
#else
#define GETSTATE(m) (&_state)
static struct module_state _state;
#endif

static PyObject *
error_out(PyObject *m __attribute__((unused))) {
    struct module_state *st = GETSTATE(m);
    PyErr_SetString(st->error, "something bad happened");
    return NULL;
}

static PyMethodDef pylibmount_methods[] = {
    {"error_out", (PyCFunction)error_out, METH_NOARGS, NULL},
    {NULL, NULL}
};

#if PY_MAJOR_VERSION >= 3

static int pylibmount_traverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}

static int pylibmount_clear(PyObject *m) {
    Py_CLEAR(GETSTATE(m)->error);
    return 0;
}

static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "pylibmount",
        NULL,
        sizeof(struct module_state),
        pylibmount_methods,
        NULL,
        pylibmount_traverse,
        pylibmount_clear,
        NULL
};
#define INITERROR return NULL
PyMODINIT_FUNC PyInit_pylibmount(void);
PyMODINIT_FUNC PyInit_pylibmount(void)
#else
#define INITERROR return
# ifndef PyMODINIT_FUNC
#  define PyMODINIT_FUNC void
# endif
PyMODINIT_FUNC initpylibmount(void);
PyMODINIT_FUNC initpylibmount(void)
#endif
{
#if PY_MAJOR_VERSION >= 3
	PyObject *m = PyModule_Create(&moduledef);
#else
	PyObject *m = Py_InitModule3("pylibmount", pylibmount_methods, PYLIBMOUNT_DESC);
#endif

	if (!m)
		INITERROR;
	/*
	 * init debug stuff
	 */
	if (!(pylibmount_debug_mask & PYMNT_DEBUG_INIT)) {
		char *str = getenv("PYLIBMOUNT_DEBUG");

		errno = 0;
		pylibmount_debug_mask = 0;
		if (str)
			pylibmount_debug_mask = strtoul(str, NULL, 0);
		if (errno)
			pylibmount_debug_mask = 0;

		pylibmount_debug_mask |= PYMNT_DEBUG_INIT;
	}

	if (pylibmount_debug_mask && pylibmount_debug_mask != PYMNT_DEBUG_INIT)
		DBG(INIT, pymnt_debug("library debug mask: 0x%04x",
					pylibmount_debug_mask));
	mnt_init_debug(0);

	/*
	 * Add module objects
	 */
	LibmountError = PyErr_NewException("libmount.Error", NULL, NULL);
	Py_INCREF(LibmountError);
	PyModule_AddObject(m, "Error", (PyObject *)LibmountError);

	FS_AddModuleObject(m);
	Table_AddModuleObject(m);
#ifdef __linux__
	Context_AddModuleObject(m);
#endif

	/*
	 * mount(8) userspace options masks (MNT_MAP_USERSPACE map)
	 */
	PyModule_AddIntConstant(m, "MNT_MS_COMMENT", MNT_MS_COMMENT);
	PyModule_AddIntConstant(m, "MNT_MS_GROUP", MNT_MS_GROUP);
	PyModule_AddIntConstant(m, "MNT_MS_HELPER", MNT_MS_HELPER);
	PyModule_AddIntConstant(m, "MNT_MS_LOOP", MNT_MS_LOOP);
	PyModule_AddIntConstant(m, "MNT_MS_NETDEV", MNT_MS_NETDEV);
	PyModule_AddIntConstant(m, "MNT_MS_NOAUTO", MNT_MS_NOAUTO);
	PyModule_AddIntConstant(m, "MNT_MS_NOFAIL", MNT_MS_NOFAIL);
	PyModule_AddIntConstant(m, "MNT_MS_OFFSET", MNT_MS_OFFSET);
	PyModule_AddIntConstant(m, "MNT_MS_OWNER", MNT_MS_OWNER);
	PyModule_AddIntConstant(m, "MNT_MS_SIZELIMIT", MNT_MS_SIZELIMIT);
	PyModule_AddIntConstant(m, "MNT_MS_ENCRYPTION", MNT_MS_ENCRYPTION);
	PyModule_AddIntConstant(m, "MNT_MS_UHELPER", MNT_MS_UHELPER);
	PyModule_AddIntConstant(m, "MNT_MS_USER", MNT_MS_USER);
	PyModule_AddIntConstant(m, "MNT_MS_USERS", MNT_MS_USERS);
	PyModule_AddIntConstant(m, "MNT_MS_XCOMMENT", MNT_MS_XCOMMENT);
	PyModule_AddIntConstant(m, "MNT_MS_HASH_DEVICE", MNT_MS_HASH_DEVICE);
	PyModule_AddIntConstant(m, "MNT_MS_ROOT_HASH", MNT_MS_ROOT_HASH);
	PyModule_AddIntConstant(m, "MNT_MS_HASH_OFFSET", MNT_MS_HASH_OFFSET);
	PyModule_AddIntConstant(m, "MNT_MS_ROOT_HASH_FILE", MNT_MS_ROOT_HASH_FILE);
	PyModule_AddIntConstant(m, "MNT_MS_FEC_DEVICE", MNT_MS_FEC_DEVICE);
	PyModule_AddIntConstant(m, "MNT_MS_FEC_OFFSET", MNT_MS_FEC_OFFSET);
	PyModule_AddIntConstant(m, "MNT_MS_FEC_ROOTS", MNT_MS_FEC_ROOTS);
	PyModule_AddIntConstant(m, "MNT_MS_ROOT_HASH_SIG", MNT_MS_ROOT_HASH_SIG);

	/*
	 * mount(2) MS_* masks (MNT_MAP_LINUX map)
	 */
	PyModule_AddIntConstant(m, "MS_BIND", MS_BIND);
	PyModule_AddIntConstant(m, "MS_DIRSYNC", MS_DIRSYNC);
	PyModule_AddIntConstant(m, "MS_I_VERSION", MS_I_VERSION);
	PyModule_AddIntConstant(m, "MS_MANDLOCK", MS_MANDLOCK);
	PyModule_AddIntConstant(m, "MS_MGC_MSK", MS_MGC_MSK);
	PyModule_AddIntConstant(m, "MS_MGC_VAL", MS_MGC_VAL);
	PyModule_AddIntConstant(m, "MS_MOVE", MS_MOVE);
	PyModule_AddIntConstant(m, "MS_NOATIME", MS_NOATIME);
	PyModule_AddIntConstant(m, "MS_NODEV", MS_NODEV);
	PyModule_AddIntConstant(m, "MS_NODIRATIME", MS_NODIRATIME);
	PyModule_AddIntConstant(m, "MS_NOEXEC", MS_NOEXEC);
	PyModule_AddIntConstant(m, "MS_NOSUID", MS_NOSUID);
	PyModule_AddIntConstant(m, "MS_OWNERSECURE", MS_OWNERSECURE);
	PyModule_AddIntConstant(m, "MS_PRIVATE", MS_PRIVATE);
	PyModule_AddIntConstant(m, "MS_PROPAGATION", MS_PROPAGATION);
	PyModule_AddIntConstant(m, "MS_RDONLY", MS_RDONLY);
	PyModule_AddIntConstant(m, "MS_REC", MS_REC);
	PyModule_AddIntConstant(m, "MS_RELATIME", MS_RELATIME);
	PyModule_AddIntConstant(m, "MS_REMOUNT", MS_REMOUNT);
	PyModule_AddIntConstant(m, "MS_SECURE", MS_SECURE);
	PyModule_AddIntConstant(m, "MS_SHARED", MS_SHARED);
	PyModule_AddIntConstant(m, "MS_SILENT", MS_SILENT);
	PyModule_AddIntConstant(m, "MS_SLAVE", MS_SLAVE);
	PyModule_AddIntConstant(m, "MS_STRICTATIME", MS_STRICTATIME);
	PyModule_AddIntConstant(m, "MS_SYNCHRONOUS", MS_SYNCHRONOUS);
	PyModule_AddIntConstant(m, "MS_UNBINDABLE", MS_UNBINDABLE);

	/* Will we need these directly?
	PyModule_AddIntConstant(m, "MNT_ERR_AMBIFS", MNT_ERR_AMBIFS);
	PyModule_AddIntConstant(m, "MNT_ERR_APPLYFLAGS", MNT_ERR_APPLYFLAGS);
	PyModule_AddIntConstant(m, "MNT_ERR_LOOPDEV", MNT_ERR_LOOPDEV);
	PyModule_AddIntConstant(m, "MNT_ERR_MOUNTOPT", MNT_ERR_MOUNTOPT);
	PyModule_AddIntConstant(m, "MNT_ERR_NOFSTAB", MNT_ERR_NOFSTAB);
	PyModule_AddIntConstant(m, "MNT_ERR_NOFSTYPE", MNT_ERR_NOFSTYPE);
	PyModule_AddIntConstant(m, "MNT_ERR_NOSOURCE", MNT_ERR_NOSOURCE);
	*/

	/* Still useful for functions using iterators internally */
	PyModule_AddIntConstant(m, "MNT_ITER_FORWARD", MNT_ITER_FORWARD);
	PyModule_AddIntConstant(m, "MNT_ITER_BACKWARD", MNT_ITER_BACKWARD);

#if PY_MAJOR_VERSION >= 3
	return m;
#endif
}

