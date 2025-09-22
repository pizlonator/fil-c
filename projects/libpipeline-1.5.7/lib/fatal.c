/*
 * fatal.c: fatal error helper
 *
 * Copyright (C) 2022 Colin Watson.
 *
 * This file is part of libpipeline.
 *
 * libpipeline is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * libpipeline is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libpipeline; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdarg.h>
#include <stdlib.h>

#include "verror.h"

#include "pipeline-private.h"

#include "fatal.h"

void fatal (int errnum, const char *format, ...)
{
	va_list ap;

	va_start (ap, format);
	verror (FATAL, errnum, format, ap);
	va_end (ap);

	/* Never reached, because verror exits if given a non-zero status,
	 * but the compiler may not be able to prove that.
	 */
	abort ();
}
