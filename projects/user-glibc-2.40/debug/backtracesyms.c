/* Return list with names for address in backtrace.
   Copyright (C) 1998-2024 Free Software Foundation, Inc.
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

#include <assert.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ldsodefs.h>
#include <stdfil.h>

#if __ELF_NATIVE_CLASS == 32
# define WORD_WIDTH 8
#else
/* We assume 64bits.  */
# define WORD_WIDTH 16
#endif


char **
__backtrace_symbols (void *const *array, int size)
{
  zerror ("backtrace_symbols not yet supported.");
  return NULL;
}
weak_alias (__backtrace_symbols, backtrace_symbols)
