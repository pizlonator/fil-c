/* Write formatted list with names for addresses in backtrace to a file.
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

#include <execinfo.h>
#include <string.h>
#include <sys/uio.h>

#include <_itoa.h>
#include <ldsodefs.h>
#include <stdfil.h>

#if __ELF_NATIVE_CLASS == 32
# define WORD_WIDTH 8
#else
/* We assume 64bits.  */
# define WORD_WIDTH 16
#endif


void
__backtrace_symbols_fd (void *const *array, int size, int fd)
{
  zprintf ("backtrace_symbols_fd not yet supported.");
}
weak_alias (__backtrace_symbols_fd, backtrace_symbols_fd)
libc_hidden_def (__backtrace_symbols_fd)
