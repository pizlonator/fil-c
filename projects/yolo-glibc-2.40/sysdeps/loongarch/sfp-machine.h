/* LoongArch softfloat definitions
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <fenv.h>
#include <fpu_control.h>

#define _FP_W_TYPE_SIZE 64
#define _FP_W_TYPE unsigned long long
#define _FP_WS_TYPE signed long long
#define _FP_I_TYPE long long

#define _FP_MUL_MEAT_S(R, X, Y) _FP_MUL_MEAT_1_imm (_FP_WFRACBITS_S, R, X, Y)
#define _FP_MUL_MEAT_D(R, X, Y) \
  _FP_MUL_MEAT_1_wide (_FP_WFRACBITS_D, R, X, Y, umul_ppmm)
#define _FP_MUL_MEAT_Q(R, X, Y) \
  _FP_MUL_MEAT_2_wide_3mul (_FP_WFRACBITS_Q, R, X, Y, umul_ppmm)

#define _FP_MUL_MEAT_DW_S(R, X, Y) \
  _FP_MUL_MEAT_DW_1_imm (_FP_WFRACBITS_S, R, X, Y)
#define _FP_MUL_MEAT_DW_D(R, X, Y) \
  _FP_MUL_MEAT_DW_1_wide (_FP_WFRACBITS_D, R, X, Y, umul_ppmm)
#define _FP_MUL_MEAT_DW_Q(R, X, Y) \
  _FP_MUL_MEAT_DW_2_wide_3mul (_FP_WFRACBITS_Q, R, X, Y, umul_ppmm)

#define _FP_DIV_MEAT_S(R, X, Y) \
  _FP_DIV_MEAT_1_imm (S, R, X, Y, _FP_DIV_HELP_imm)
#define _FP_DIV_MEAT_D(R, X, Y) _FP_DIV_MEAT_1_udiv_norm (D, R, X, Y)
#define _FP_DIV_MEAT_Q(R, X, Y) _FP_DIV_MEAT_2_udiv (Q, R, X, Y)

#define _FP_NANFRAC_S _FP_QNANBIT_S
#define _FP_NANFRAC_D _FP_QNANBIT_D
#define _FP_NANFRAC_Q _FP_QNANBIT_Q, 0

#define _FP_NANSIGN_S 0
#define _FP_NANSIGN_D 0
#define _FP_NANSIGN_Q 0

#define _FP_KEEPNANFRACP 1
#define _FP_QNANNEGATEDP 0

/* NaN payloads should be preserved for NAN2008.  */
#define _FP_CHOOSENAN(fs, wc, R, X, Y, OP) \
  do \
    { \
      R##_s = X##_s; \
      _FP_FRAC_COPY_##wc (R, X); \
      R##_c = FP_CLS_NAN; \
    } \
  while (0)

#define _FP_DECL_EX fpu_control_t _fcw

#define FP_ROUNDMODE (_fcw & 0x300)

#define FP_RND_NEAREST FE_TONEAREST
#define FP_RND_ZERO FE_TOWARDZERO
#define FP_RND_PINF FE_UPWARD
#define FP_RND_MINF FE_DOWNWARD

#define FP_EX_INVALID FE_INVALID
#define FP_EX_OVERFLOW FE_OVERFLOW
#define FP_EX_UNDERFLOW FE_UNDERFLOW
#define FP_EX_DIVZERO FE_DIVBYZERO
#define FP_EX_INEXACT FE_INEXACT

#define _FP_TININESS_AFTER_ROUNDING 1

#ifdef __loongarch_hard_float
#define FP_INIT_ROUNDMODE \
  do \
    { \
      _FPU_GETCW (_fcw); \
    } \
  while (0)

#define FP_HANDLE_EXCEPTIONS \
  do \
    { \
      if (__glibc_unlikely (_fex)) \
	_FPU_SETCW (_fcw | _fex | (_fex << 8)); \
    } \
  while (0)
#define FP_TRAPPING_EXCEPTIONS ((_fcw << 16) & 0x1f0000)
#else
#define FP_INIT_ROUNDMODE _fcw = FP_RND_NEAREST
#endif
