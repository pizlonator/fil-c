/* RISC-V RVV based stpncpy.
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

#ifndef USE_AS_STRNCPY
# ifndef STPNCPY
#  ifdef weak_alias
#   define STPNCPY __stpncpy
weak_alias (__stpncpy, stpncpy)
#  else
#   define STPNCPY stpncpy
#  endif
# endif
#else
# ifndef STRNCPY
#  define STPNCPY strncpy
# else
#  define STPNCPY STRNCPY
# endif
#endif

#define dst a0
#define src a1
#define length a2
#define dst_ptr a3
#define active_elem_pos a4
#define cur_vl a5
#define ivl a6
#define temp a1

#define ELEM_LMUL_SETTING m1
#define vmask1 v0
#define vmask2 v1
#define ZERO_FILL_ELEM_LMUL_SETTING m8
#define vstr1 v8
#define vstr2 v16

ENTRY (STPNCPY)
.option push
.option arch, +v
    mv dst_ptr, dst
    /* Copy src to dst_ptr.  */
L(stpcpy_loop):
    vsetvli zero, length, e8, ELEM_LMUL_SETTING, ta, ma
    vle8ff.v vstr1, (src)
    vmseq.vx vmask2, vstr1, zero
    csrr cur_vl, vl
    vfirst.m active_elem_pos, vmask2
    vmsif.m vmask1, vmask2
    add src, src, cur_vl
    sub length, length, cur_vl
    vse8.v vstr1, (dst_ptr), vmask1.t
    add dst_ptr, dst_ptr, cur_vl
    bgez active_elem_pos, L(fill_zero)
    bnez length, L(stpcpy_loop)
#ifndef USE_AS_STRNCPY
    mv dst, dst_ptr
#endif
    ret

    /* Fill the tail zero.  */
L(fill_zero):
    /* We already copied the `\0` to dst, but we use `vfirst.m` to
       get the `index` of the `\0` position. We need to adjust by `-1`
       to get the correct remaining length for zero filling.  */
    sub temp, cur_vl, active_elem_pos
    addi temp, temp, -1
#ifndef USE_AS_STRNCPY
    sub dst, dst_ptr, cur_vl
    add dst, dst, active_elem_pos
#endif
    add length, length, temp
    /* Return early for the `strlen(src) + 1 == count` case.  */
    bnez length, L(do_fill_zero)
    ret

L(do_fill_zero):
    sub dst_ptr, dst_ptr, temp
    vsetvli zero, length, e8, ZERO_FILL_ELEM_LMUL_SETTING, ta, ma
    vmv.v.x vstr2, zero
L(fill_zero_loop):
    vsetvli ivl, length, e8, ZERO_FILL_ELEM_LMUL_SETTING, ta, ma
    vse8.v vstr2, (dst_ptr)
    sub length, length, ivl
    add dst_ptr, dst_ptr, ivl
    bnez length, L(fill_zero_loop)
    ret
.option pop
END (STPNCPY)

#ifndef USE_AS_STRNCPY
# ifdef weak_alias
libc_hidden_def (__stpncpy)
# endif
#else
libc_hidden_builtin_def (strncpy)
#endif
