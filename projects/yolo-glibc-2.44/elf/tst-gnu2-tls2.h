/* Test TLSDESC relocation.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <stdint.h>

struct tls
{
  int64_t a, b, c, d;
};

extern struct tls *apply_tls (struct tls *);

/* An architecture can define them to verify that clobber caller-saved
   registers aren't changed by the implicit TLSDESC call.  */
#ifndef INIT_TLSDESC_CALL
# define INIT_TLSDESC_CALL()
#endif

#ifndef BEFORE_TLSDESC_CALL
# define BEFORE_TLSDESC_CALL()
#endif

#ifndef AFTER_TLSDESC_CALL
# define AFTER_TLSDESC_CALL()
#endif
