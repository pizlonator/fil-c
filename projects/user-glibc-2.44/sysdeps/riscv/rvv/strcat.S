/* RISC-V RVV based strcat.
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

#ifndef STRCAT
# define STRCAT strcat
#endif

#define dst a0
#define src a1
#define dst_ptr a2

#define ivl a3
#define cur_vl a4
#define active_elem_pos a5

#define ELEM_LMUL_SETTING m1
#define vmask1 v0
#define vmask2 v1
#define vstr1 v8
#define vstr2 v16

ENTRY (STRCAT)
.option push
.option arch, +v
    mv dst_ptr, dst
    /* Perform `strlen(dst)`.  */
L(strlen_loop):
    vsetvli ivl, zero, e8, ELEM_LMUL_SETTING, ta, ma

    vle8ff.v vstr1, (dst_ptr)
    vmseq.vx vmask1, vstr1, zero
    csrr cur_vl, vl
    vfirst.m active_elem_pos, vmask1
    add dst_ptr, dst_ptr, cur_vl
    bltz active_elem_pos, L(strlen_loop)

    sub dst_ptr, dst_ptr, cur_vl
    add dst_ptr, dst_ptr, active_elem_pos

    /* Perform `strcpy(dst, src)`.  */
L(strcpy_loop):
    vsetvli ivl, zero, e8, ELEM_LMUL_SETTING, ta, ma

    vle8ff.v vstr1, (src)
    vmseq.vx vmask2, vstr1, zero
    csrr cur_vl, vl
    vfirst.m active_elem_pos, vmask2
    vmsif.m vmask1, vmask2
    add src, src, cur_vl
    vse8.v vstr1, (dst_ptr), vmask1.t
    add dst_ptr, dst_ptr, cur_vl
    bltz active_elem_pos, L(strcpy_loop)

    ret
.option pop
END (STRCAT)
libc_hidden_builtin_def (strcat)
