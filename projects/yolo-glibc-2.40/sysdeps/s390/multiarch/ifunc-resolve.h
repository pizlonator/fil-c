/* IFUNC resolver function for CPU specific functions.
   32/64 bit S/390 version.
   Copyright (C) 2015-2024 Free Software Foundation, Inc.
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

#include <unistd.h>
#include <dl-procinfo.h>
#include <cpu-features.h>

#define s390_libc_ifunc_expr_stfle_init()				\
  const unsigned long long *stfle_bits = features->stfle_bits;

#define s390_libc_ifunc_expr_init()					\
  const struct cpu_features *features = &GLRO(dl_s390_cpu_features);	\
  /* The hwcap from kernel is passed as argument, but we		\
     explicitly use the hwcaps from cpu-features struct.   */		\
  hwcap = features->hwcap;

#define s390_libc_ifunc_expr(TYPE_FUNC, FUNC, EXPR)		\
  __ifunc (TYPE_FUNC, FUNC, EXPR, unsigned long int hwcap,	\
	   s390_libc_ifunc_expr_init);
