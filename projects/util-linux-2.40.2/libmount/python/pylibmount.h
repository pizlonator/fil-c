#ifndef UTIL_LINUX_PYLIBMOUNT_H
#define UTIL_LINUX_PYLIBMOUNT_H

#include <Python.h>
#include <structmember.h>

#include "c.h"
#include "libmount.h"

#define CONFIG_PYLIBMOUNT_DEBUG

#define PYMNT_DEBUG_INIT	(1 << 1)
#define PYMNT_DEBUG_TAB		(1 << 2)
#define PYMNT_DEBUG_FS		(1 << 3)
#define PYMNT_DEBUG_CXT		(1 << 4)

#ifdef CONFIG_PYLIBMOUNT_DEBUG
# include <stdio.h>
# include <stdarg.h>

# define DBG(m, x)	do { \
				if ((PYMNT_DEBUG_ ## m) & pylibmount_debug_mask) { \
					fprintf(stderr, "%d: pylibmount: %6s: ", getpid(), # m); \
					x; \
				} \
			} while (0)

extern int pylibmount_debug_mask;

static inline void __attribute__ ((__format__ (__printf__, 1, 2)))
pymnt_debug(const char *mesg, ...)
{
	va_list ap;
	va_start(ap, mesg);
	vfprintf(stderr, mesg, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static inline void __attribute__ ((__format__ (__printf__, 2, 3)))
pymnt_debug_h(void *handler, const char *mesg, ...)
{
	va_list ap;

	if (handler)
		fprintf(stderr, "[%p]: ", handler);
	va_start(ap, mesg);
	vfprintf(stderr, mesg, ap);
	va_end(ap);
	fputc('\n', stderr);
}

#else /* !CONFIG_PYLIBMOUNT_DEBUG */
# define DBG(m,x) do { ; } while (0)
#endif


#define NODEL_ATTR	"This attribute cannot be deleted"
#define CONSTRUCT_ERR	"Error during object construction"
#define ARG_ERR		"Invalid number or type of arguments"
#define NOFS_ERR	"No filesystems to mount"
#define MEMORY_ERR	strerror(ENOMEM)
#define CONV_ERR	"Type conversion failed"

/*
 * fs.c
 */
typedef struct {
	PyObject_HEAD
	struct libmnt_fs *fs;
} FsObject;

extern PyTypeObject FsType;

extern PyObject *PyObjectResultFs(struct libmnt_fs *fs);
extern void FS_AddModuleObject(PyObject *mod);

/*
 * tab.c
 */
typedef struct {
	PyObject_HEAD

	struct libmnt_table		*tab;
	struct libmnt_iter		*iter;
	PyObject			*errcb;
} TableObject;

extern PyTypeObject TableType;

extern PyObject *PyObjectResultTab(struct libmnt_table *tab);

extern void Table_unref(struct libmnt_table *tab);
extern void Table_AddModuleObject(PyObject *mod);

extern int pymnt_table_parser_errcb(struct libmnt_table *tb, const char *filename, int line);

#ifdef __linux__

/*
 * context.c
 */
typedef struct {
	PyObject_HEAD

	struct libmnt_context		*cxt;
	PyObject			*table_errcb;

} ContextObjext;

extern PyTypeObject ContextType;
extern void Context_AddModuleObject(PyObject *mod);

#endif /* __linux__ */

/*
 * misc
 */
extern PyObject *LibmountError;
extern PyObject *UL_IncRef(void *killme);
extern void *UL_RaiseExc(int e);

extern PyObject *PyObjectResultInt(int i);
extern PyObject *PyObjectResultStr(const char *s);

extern char *pystos(PyObject *pys);
extern void PyFree(void *o);



#endif /* UTIL_LINUX_PYLIBMOUNT */
