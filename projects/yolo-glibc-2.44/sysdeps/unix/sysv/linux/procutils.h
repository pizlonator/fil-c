/* Utilities functions to read/parse Linux procfs and sysfs.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _PROCUTILS_H
#define _PROCUTILS_H

#include <stdbool.h>

typedef int (*procutils_closure_t) (const char *line, void *arg);

#define PROCUTILS_MAX_LINE_LEN 256

/* Open and read the path FILENAME, line per line, and call CLOSURE with
   argument ARG on each line.  The read is done with a static buffer,
   with non-cancellable calls, and the line is null terminated.

   The CLOSURE should return 0 if the read should continue, otherwise the
   the function should stop and return early.

   The '\n' is not included in the CLOSURE input argument and lines longer
   than PROCUTILS_MAX_LINE_LEN characteres are ignored.

   It returns true in case the file is fully read or false if CLOSURE
   returns a value diferent than 0.  */
bool __libc_procutils_read_file (const char *filename,
				 procutils_closure_t closure,
				 void *arg) attribute_hidden;

#endif
