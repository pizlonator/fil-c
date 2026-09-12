/* FPU control word bits.  OpenRISC version.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _FPU_CONTROL_H
#define _FPU_CONTROL_H

#ifndef __or1k_hard_float__

# define _FPU_RESERVED 0xffffffff
# define _FPU_DEFAULT  0x00000000
# define _FPU_GETCW(cw) (cw) = 0
# define _FPU_SETCW(cw) (void) (cw)

#else /* __or1k_hard_float__ */

/* Layout of FPCSR:

   The bits of the FPCSR are defined as follows, this should help
   explain how the masks below have come to be.

   +-----------+----------------------------+-----+----+
   |  32 - 12  | 11 10  9  8  7  6  5  4  3 | 2-1 |  0 |
   +-----------+----------------------------+-----+----+
   |  Reserved | DZ IN IV IX  Z QN SN UN OV | RM  | EE |
   +-----------+----------------------------+-----+----+

   Exception flags:

     DZ - divide by zero flag.
     IN - infinite flag.
     IV - invalid flag.
     IX - inexact flag.
      Z - zero flag.
     QN - qnan flag.
     SN - snan flag.
     UN - underflow flag.
     OV - overflow flag.

   Rounding modes:

   The FPCSR bits 2-1 labeled above as RM specify the rounding mode.

     00 - round to nearest
     01 - round to zero
     10 - round to positive infinity
     11 - round to negative infinity

   Enabling exceptions:

     EE - set to enable FPU exceptions.

 */

# define _FPU_RESERVED 0xfffff000
/* Default: rounding to nearest with exceptions disabled.  */
# define _FPU_DEFAULT  0
/* IEEE: Same as above with exceptions enabled.  */
# define _FPU_IEEE     (_FPU_DEFAULT | 1)

# define _FPU_FPCSR_RM_MASK (0x3 << 1)

/* Macros for accessing the hardware control word.  */
# define _FPU_GETCW(cw) __asm__ volatile ("l.mfspr %0,r0,20" : "=r" (cw))
# define _FPU_SETCW(cw) __asm__ volatile ("l.mtspr r0,%0,20" : : "r" (cw))

#endif /* __or1k_hard_float__ */

/* Type of the control word.  */
typedef unsigned int fpu_control_t;

/* Default control word set at startup.  */
extern fpu_control_t __fpu_control;

#endif	/* fpu_control.h */
