/* Floating point environment, OpenRISC version.
   Copyright (C) 2022-2026 Free Software Foundation, Inc.

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _FENV_H
# error "Never use <bits/fenv.h> directly; include <fenv.h> instead."
#endif

#ifdef __or1k_hard_float__
/* Define bits representing exceptions in the FPCSR status word.  */
enum
  {
    FE_OVERFLOW =
#define FE_OVERFLOW	(1 << 3)
      FE_OVERFLOW,
    FE_UNDERFLOW =
#define FE_UNDERFLOW	(1 << 4)
      FE_UNDERFLOW,
    FE_INEXACT =
#define FE_INEXACT	(1 << 8)
      FE_INEXACT,
    FE_INVALID =
#define FE_INVALID	(1 << 9)
      FE_INVALID,
    FE_DIVBYZERO =
#define FE_DIVBYZERO	(1 << 11)
      FE_DIVBYZERO,
  };

/* All supported exceptions.  */
#define FE_ALL_EXCEPT	\
	(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT)

/* Define bits representing rounding modes in the FPCSR Rmode field.  */
#define FE_TONEAREST  (0x0 << 1)
#define FE_TOWARDZERO (0x1 << 1)
#define FE_UPWARD     (0x2 << 1)
#define FE_DOWNWARD   (0x3 << 1)

#else

/* In the soft-float case only rounding to nearest is supported, with
   no exceptions.  */

enum
  {
    __FE_UNDEFINED = -1,

    FE_TONEAREST =
# define FE_TONEAREST	0x0
      FE_TONEAREST
  };

# define FE_ALL_EXCEPT 0

#endif /* __or1k_hard_float__ */

/* Type representing exception flags.  */
typedef unsigned int fexcept_t;

/* Type representing floating-point environment.  */
typedef unsigned int fenv_t;

/* If the default argument is used we use this value.  */
#define FE_DFL_ENV	((const fenv_t *) -1l)

#if __GLIBC_USE (IEC_60559_BFP_EXT_C23)
/* Type representing floating-point control modes.  */
typedef unsigned int femode_t;

/* Default floating-point control modes.  */
# define FE_DFL_MODE	((const femode_t *) -1L)
#endif
