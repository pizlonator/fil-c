/* Test TLSDESC relocation, x86 version.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#ifndef TEST_AMX
# include <support/check.h>

typedef long long v2di __attribute__((vector_size(16)));
extern v2di v1, v2, v3;

# define BEFORE_TLSDESC_CALL()					\
  v1 = __extension__(v2di){0, 0};				\
  v2 = __extension__(v2di){0, 0};

# define AFTER_TLSDESC_CALL()					\
  v3 = __extension__(v2di){0, 0};				\
  asm volatile ("" : "+x" (v3));				\
  union { v2di x; long long a[2]; } u;				\
  u.x = v3;							\
  TEST_VERIFY_EXIT (u.a[0] == 0 && u.a[1] == 0);
#endif

#include <elf/tst-gnu2-tls2.h>
