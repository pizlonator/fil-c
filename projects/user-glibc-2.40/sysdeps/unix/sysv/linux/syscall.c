/* Indirect system call.  Linux generic implementation.
   Copyright (C) 1997-2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <stdarg.h>
#include <sysdep.h>

long int
syscall (long int number, ...)
{
  va_list args;

  va_start (args, number);
  long int a0 = va_arg (args, long int);
  long int a1 = va_arg (args, long int);
  long int a2 = va_arg (args, long int);
  long int a3 = va_arg (args, long int);
  long int a4 = va_arg (args, long int);
  long int a5 = va_arg (args, long int);
  va_end (args);

  long int r = INTERNAL_SYSCALL_NCS_CALL (number, a0, a1, a2, a3, a4, a5);
  if (__glibc_unlikely (INTERNAL_SYSCALL_ERROR_P (r)))
    {
      __set_errno (-r);
      return -1;
    }
  return r;
}
