/*
 * SPDX-FileCopyrightText: 2007 - 2009, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * According to the Linux-PAM documentation:
 *
 *  4.1. Care about standard library calls
 *
 *  In general, writers of authorization-granting applications should
 *  assume that each module is likely to call any or all 'libc' functions.
 *  For 'libc' functions that return pointers to static/dynamically
 *  allocated structures (ie.  the library allocates the memory and the
 *  user is not expected to 'free()' it) any module call to this function
 *  is likely to corrupt a pointer previously obtained by the application.
 *  The application programmer should either re-call such a 'libc'
 *  function after a call to the Linux-PAM library, or copy the structure
 *  contents to some safe area of memory before passing control to the
 *  Linux-PAM library.
 *
 *  Two important function classes that fall into this category are
 *  getpwnam(3) and syslog(3).
 *
 * This file provides wrapper to the getpwnam or getpwnam_r functions.
 */

#include <config.h>

#include "pwio.h"

#define LOOKUP_TYPE	struct passwd
#define FUNCTION_NAME	getpwnam
#define ARG_TYPE	const char *
#define ARG_NAME	name
#define DUP_FUNCTION	__pw_dup
#define HAVE_FUNCTION_R 1

#include "xgetXXbyYY.c"

