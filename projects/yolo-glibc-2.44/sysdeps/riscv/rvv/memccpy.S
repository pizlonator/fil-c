/* RISC-V RVV based memccpy.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include <sysdep.h>
#include <sys/asm.h>

#ifndef MEMCCPY
# define MEMCCPY __memccpy
#endif

#define dst a0
#define src a1
#define c_val a2
#define num a3

#define ivl a4
#define dst_ptr a5
#define vfirst_idx a6

#define ELEM_LMUL_SETTING m8
#define vdata v0
#define vmask v8

ENTRY (MEMCCPY)
.option push
.option arch, +v
    mv dst_ptr, dst
L(loop):
    vsetvli zero, num, e8, ELEM_LMUL_SETTING, ta, ma
    vle8ff.v vdata, (src)

    vmseq.vx vmask, vdata, c_val
    vfirst.m vfirst_idx, vmask
    csrr ivl, vl
    bgez vfirst_idx, L(found)
    vse8.v vdata, (dst_ptr)

    sub num, num, ivl
    add src, src, ivl
    add dst_ptr, dst_ptr, ivl
    bnez num, L(loop)
    mv a0, zero
    ret
L(found):
    addi vfirst_idx, vfirst_idx, 1
    vsetvli zero, vfirst_idx, e8, ELEM_LMUL_SETTING, ta, ma
    vse8.v vdata, (dst_ptr)
    add a0, dst_ptr, vfirst_idx
    ret
.option pop
END (MEMCCPY)
weak_alias (MEMCCPY, memccpy)
