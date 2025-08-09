/*
 * stdlib.h --
 *
 *	Declares facilities exported by the "stdlib" portion of the C library.
 *	This file isn't complete in the ANSI-C sense; it only declares things
 *	that are needed by Tcl. This file is needed even on many systems with
 *	their own stdlib.h (e.g. SunOS) because not all stdlib.h files declare
 *	all the procedures needed here (such as strtol/strtoul).
 *
 * Copyright (c) 1991 The Regents of the University of California.
 * Copyright (c) 1994-1998 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _STDLIB
#define _STDLIB

extern void		abort(void);
extern double		atof(const char *string);
extern int		atoi(const char *string);
extern long		atol(const char *string);
extern void *		calloc(unsigned long numElements, unsigned long size);
extern void		exit(int status);
extern void		free(void *blockPtr);
extern char *		getenv(const char *name);
extern void *		malloc(unsigned long numBytes);
extern void		qsort(void *base, unsigned long n, unsigned long size, int (*compar)(
			    const void *element1, const void *element2));
extern void *		realloc(void *ptr, unsigned long numBytes);
extern char *		realpath(const char *path, char *resolved_path);
extern int		mkstemps(char *templ, int suffixlen);
extern int		mkstemp(char *templ);
extern char *		mkdtemp(char *templ);
extern long		strtol(const char *string, char **endPtr, int base);
extern unsigned long	strtoul(const char *string, char **endPtr, int base);

#endif /* _STDLIB */
