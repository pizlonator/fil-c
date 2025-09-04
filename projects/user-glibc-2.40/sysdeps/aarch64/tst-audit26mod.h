/* Check LD_AUDIT for aarch64 specific ABI.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#ifndef _TST_AUDIT27MOD_H
#define _TST_AUDIT27MOD_H 1

#include <array_length.h>

struct large_struct
{
  char a[16];
  short b[8];
  long int c[4];
};

static inline struct large_struct
set_large_struct (char a, short b, long int c)
{
  struct large_struct r;
  for (int i = 0; i < array_length (r.a); i++)
    r.a[i] = a;
  for (int i = 0; i < array_length (r.b); i++)
    r.b[i] = b;
  for (int i = 0; i < array_length (r.c); i++)
    r.c[i] = c;
  return r;
}

#define ARG1 0x12
#define ARG2 0x1234
#define ARG3 0x12345678

struct large_struct tst_audit26_func (char a, short b, long int c);

#endif
