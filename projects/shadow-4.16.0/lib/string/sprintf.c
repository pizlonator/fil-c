/*
 * SPDX-FileCopyrightText: 2023, Alejandro Colomar <alx@kernel.org>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include "string/sprintf.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>


extern inline int xasprintf(char **restrict s, const char *restrict fmt, ...);
extern inline int xvasprintf(char **restrict s, const char *restrict fmt,
    va_list ap);

extern inline int snprintf_(char *restrict s, size_t size,
    const char *restrict fmt, ...);
extern inline int vsnprintf_(char *restrict s, size_t size,
    const char *restrict fmt, va_list ap);
