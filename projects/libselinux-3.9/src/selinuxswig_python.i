/* Author: James Athey
 */

/* Never build rpm_execcon interface */
#ifndef DISABLE_RPM
#define DISABLE_RPM
#endif

%module selinux
%{
	#include "selinux/selinux.h"
%}

%pythoncode %{

import shutil
import os

DISABLED = -1
PERMISSIVE = 0
ENFORCING = 1

def restorecon(path, recursive=False, verbose=False, force=False, nthreads=1):
    """ Restore SELinux context on a given path

    Arguments:
    path -- The pathname for the file or directory to be relabeled.

    Keyword arguments:
    recursive -- Change files and directories file labels recursively (default False)
    verbose -- Show changes in file labels (default False)
    force -- Force reset of context to match file_context for customizable files,
    and the default file context, changing the user, role, range portion  as well
    as the type (default False)
    nthreads -- The number of threads to use during relabeling, or 0 to use as many
    threads as there are online CPU cores (default 1)
    """

    restorecon_flags = SELINUX_RESTORECON_IGNORE_DIGEST | SELINUX_RESTORECON_REALPATH
    if recursive:
        restorecon_flags |= SELINUX_RESTORECON_RECURSE
    if verbose:
        restorecon_flags |= SELINUX_RESTORECON_VERBOSE
    if force:
        restorecon_flags |= SELINUX_RESTORECON_SET_SPECFILE_CTX
    selinux_restorecon_parallel(os.path.expanduser(path), restorecon_flags, nthreads)

def chcon(path, context, recursive=False):
    """ Set the SELinux context on a given path """
    lsetfilecon(path, context)
    if recursive:
        for root, dirs, files in os.walk(path):
            for name in files + dirs:
                lsetfilecon(os.path.join(root, name), context)

def copytree(src, dest):
    """ An SELinux-friendly shutil.copytree method """
    shutil.copytree(src, dest)
    restorecon(dest, recursive=True)

def install(src, dest):
    """ An SELinux-friendly shutil.move method """
    shutil.move(src, dest)
    restorecon(dest, recursive=True)
%}

/* security_get_boolean_names() typemap */
%typemap(argout) (char ***names, int *len) {
	PyObject* list = PyList_New(*$2);
	int i;
	for (i = 0; i < *$2; i++) {
		PyList_SetItem(list, i, PyString_FromString((*$1)[i]));
	}
	$result = SWIG_AppendOutput($result, list);
}

/* return a sid along with the result */
%typemap(argout) (security_id_t * sid) {
	if (*$1) {
                %append_output(SWIG_NewPointerObj(*$1, $descriptor(security_id_t), 0));
	} else {
		Py_INCREF(Py_None);
		%append_output(Py_None);
	}
}

%typemap(in,numinputs=0) security_id_t *(security_id_t temp) {
  $1 = &temp;
}

%typemap(in, numinputs=0) void *(char *temp=NULL) {
	$1 = temp;
}

/* Makes security_compute_user() return a Python list of contexts */
%typemap(argout) (char ***con) {
	PyObject* plist;
	int i, len = 0;
	
	if (*$1) {
		while((*$1)[len])
			len++;
		plist = PyList_New(len);
		for (i = 0; i < len; i++) {
			PyList_SetItem(plist, i, PyString_FromString((*$1)[i]));
		}
	} else {
		plist = PyList_New(0);
	}

	$result = SWIG_AppendOutput($result, plist);
}

/* Makes functions in get_context_list.h return a Python list of contexts */
%typemap(argout) (char ***list) {
	PyObject* plist;
	int i;
	
	if (*$1) {
		plist = PyList_New(result);
		for (i = 0; i < result; i++) {
			PyList_SetItem(plist, i, PyString_FromString((*$1)[i]));
		}
	} else {
		plist = PyList_New(0);
	}
	/* Only return the Python list, don't need to return the length anymore */
	$result = plist;
}

%typemap(in,noblock=1,numinputs=0) char ** (char * temp = 0) {
	$1 = &temp;
}
%typemap(freearg,match="in") char ** "";
%typemap(argout,noblock=1) char ** {
	if (*$1) {
		%append_output(SWIG_FromCharPtr(*$1));
		freecon(*$1);
	}
	else {
		Py_INCREF(Py_None);
		%append_output(Py_None);
	}
}

%typemap(in,noblock=1,numinputs=0) char ** (char * temp = 0) {
	$1 = &temp;
}
%typemap(freearg,match="in") char ** "";
%typemap(argout,noblock=1) char ** {
	if (*$1) {
		%append_output(SWIG_FromCharPtr(*$1));
		free(*$1);
	}
	else {
		Py_INCREF(Py_None);
		%append_output(Py_None);
	}
}

%include "selinuxswig_python_exception.i"
%include "selinuxswig.i"
