/*
 * SPDX-FileCopyrightText:  2022 - 2023, Alejandro Colomar <alx@kernel.org>
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 */


#include <config.h>

#if !defined(HAVE_STPEPRINTF)

#ident "$Id$"

#include "string/stpeprintf.h"

#include <stdarg.h>


extern inline char *stpeprintf(char *dst, char *end, const char *restrict fmt,
    ...);
extern inline char *vstpeprintf(char *dst, char *end, const char *restrict fmt,
    va_list ap);


#endif  // !HAVE_STPEPRINTF
