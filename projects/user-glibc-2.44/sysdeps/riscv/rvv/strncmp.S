/* RISC-V RVV based strncmp.
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

#ifndef STRNCMP
# define STRNCMP strncmp
#endif

#define result a0

#define str1 a0
#define str2 a1
#define length a2

#define ivl a3
#define temp1 a4
#define temp2 a5

#define ELEM_LMUL_SETTING m1
#define vstr1 v0
#define vstr2 v4
#define vmask1 v8
#define vmask2 v9

ENTRY (STRNCMP)
.option push
.option arch, +v
    beqz length, L(zero_length)
L(loop):
    vsetvli zero, length, e8, ELEM_LMUL_SETTING, ta, ma

    vle8ff.v vstr1, (str1)
    /* vstr1[i] == 0.  */
    vmseq.vx vmask1, vstr1, zero

    vle8ff.v vstr2, (str2)
    /* vstr1[i] != vstr2[i].  */
    vmsne.vv vmask2, vstr1, vstr2

    csrr ivl, vl

    /* r = mask1 | mask2
    We can use vfirst.m to get the first zero char or the
    first different char between str1 and str2. */
    vmor.mm vmask1, vmask1, vmask2

    sub length, length, ivl

    vfirst.m temp1, vmask1

    bgez temp1, L(end_loop)

    add str1, str1, ivl
    add str2, str2, ivl
    bnez length, L(loop)

L(end_loop):
    add str1, str1, temp1
    add str2, str2, temp1
    lbu temp1, 0(str1)
    lbu temp2, 0(str2)

    sub result, temp1, temp2
    ret

L(zero_length):
    li result, 0
    ret
.option pop
END (STRNCMP)
libc_hidden_builtin_def (strncmp)
