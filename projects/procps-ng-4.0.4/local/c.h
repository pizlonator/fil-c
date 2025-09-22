/*
 * This header was copied from util-linux at fall 2011.
 */

/*
 * Fundamental C definitions.
 */

#ifndef PROCPS_NG_C_H
#define PROCPS_NG_C_H

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_ERROR_H
#include <error.h>
#else
#include <stdarg.h>
#endif

/*
 * Compiler specific stuff
 */
#ifndef __GNUC_PREREQ
# if defined __GNUC__ && defined __GNUC_MINOR__
#  define __GNUC_PREREQ(maj, min) \
	((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
# else
#  define __GNUC_PREREQ(maj, min) 0
# endif
#endif

/*
 * Function attributes
 */
#ifndef __ul_alloc_size
# if __GNUC_PREREQ (4, 3)
#  define __ul_alloc_size(s) __attribute__((alloc_size(s)))
# else
#  define __ul_alloc_size(s)
# endif
#endif

#ifndef __ul_calloc_size
# if __GNUC_PREREQ (4, 3)
#  define __ul_calloc_size(n, s) __attribute__((alloc_size(n, s)))
# else
#  define __ul_calloc_size(n, s)
# endif
#endif

/*
 * Misc
 */
#ifndef PATH_MAX
# define PATH_MAX 4096
#endif

#ifndef TRUE
# define TRUE 1
#endif

#ifndef FALSE
# define FALSE 0
#endif

/*
 * Program name.
 */
#ifndef HAVE_PROGRAM_INVOCATION_SHORT_NAME
# ifdef HAVE___PROGNAME
extern char *__progname;
#  define program_invocation_short_name __progname
# else
#  ifdef HAVE_GETEXECNAME
#   define program_invocation_short_name \
		prog_inv_sh_nm_from_file(getexecname(), 0)
#  else
#   define program_invocation_short_name \
		prog_inv_sh_nm_from_file(__FILE__, 1)
#  endif
static char prog_inv_sh_nm_buf[256];
static inline char *prog_inv_sh_nm_from_file(char *f, char stripext)
{
	char *t;

	if ((t = strrchr(f, '/')) != NULL)
		t++;
	else
		t = f;

	strncpy(prog_inv_sh_nm_buf, t, sizeof(prog_inv_sh_nm_buf) - 1);
	prog_inv_sh_nm_buf[sizeof(prog_inv_sh_nm_buf) - 1] = '\0';

	if (stripext && (t = strrchr(prog_inv_sh_nm_buf, '.')) != NULL)
		*t = '\0';

	return prog_inv_sh_nm_buf;
}
# endif
#endif

/*
 * Error printing.
 */
#ifndef HAVE_ERROR_H
/* Emulate the error() function from glibc */
__attribute__((__format__(__printf__, 3, 4)))
static void error(int status, int errnum, const char *format, ...)
{
        va_list argp;
        fprintf(stderr, "%s: ", program_invocation_short_name);
        va_start(argp, format);
        vfprintf(stderr, format, argp);
        va_end(argp);
        if (errnum != 0)
                fprintf(stderr, ": %s", strerror(errnum));
        fprintf(stderr, "\n");
        if (status != 0)
                exit(status);
}

/* Emulate the error_at_line() function from glibc */
__attribute__((__format__(__printf__, 5, 6)))
static void error_at_line(int status, int errnum, const char *filename,
                          unsigned int linenum, const char *format, ...)
{
        va_list argp;
        fprintf(stderr, "%s:%s:%u: ", program_invocation_short_name,
                filename, linenum);
        va_start(argp, format);
        vfprintf(stderr, format, argp);
        va_end(argp);
        if (errnum != 0)
                fprintf(stderr, ": error code %d", errnum);
        fprintf(stderr, "\n");
        if (status != 0)
                exit(status);
}
#endif
#define xwarn(...) error(0, errno, __VA_ARGS__)
#define xwarnx(...) error(0, 0, __VA_ARGS__)
#define xerr(STATUS, ...) error(STATUS, errno, __VA_ARGS__)
#define xerrx(STATUS, ...) error(STATUS, 0, __VA_ARGS__)

/*
 * Constant strings for usage() functions.
 */
#define USAGE_HEADER     _("\nUsage:\n")
#define USAGE_OPTIONS    _("\nOptions:\n")
#define USAGE_SEPARATOR  _("\n")
#define USAGE_HELP       _(" -h, --help     display this help and exit\n")
#define USAGE_VERSION    _(" -V, --version  output version information and exit\n")
#define USAGE_MAN_TAIL(_man)   _("\nFor more details see %s.\n"), _man

#define PROCPS_NG_VERSION _("%s from %s\n"), program_invocation_short_name, PACKAGE_STRING

#endif /* PROCPS_NG_C_H */
