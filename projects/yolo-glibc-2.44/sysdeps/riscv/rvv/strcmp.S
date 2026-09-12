/* RISC-V RVV based strcmp.
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

#ifndef STRCMP
# define STRCMP strcmp
#endif

#define result a0

#define str1 a0
#define str2 a1

#define ivl a2
#define temp1 a3
#define temp2 a4

#define vstr1 v0
#define vstr2 v8
#define vmask1 v16
#define vmask2 v17

ENTRY (STRCMP)
.option push
.option arch, +v
    /* lmul=1 */
L(Loop):
    vsetvli ivl, zero, e8, m1, ta, ma
    vle8ff.v vstr1, (str1)
    /* Check if vstr1[i] == 0 */
    vmseq.vx vmask1, vstr1, zero

    vle8ff.v vstr2, (str2)
    /* Check if vstr1[i] != vstr2[i] */
    vmsne.vv vmask2, vstr1, vstr2

    /* Find the index x for vstr1[x] == 0 */
    vfirst.m temp1, vmask1
    /* Find the index x for vstr1[x] != vstr2[x] */
    vfirst.m temp2, vmask2

    bgez temp1, L(check1)
    bgez temp2, L(check2)

    /* Get the current vl updated by vle8ff. */
    csrr ivl, vl
    add str1, str1, ivl
    add str2, str2, ivl
    j L(Loop)

    /* temp1 >= 0 */
L(check1):
    bltz temp2, L(return_at_nul)
    blt temp2, temp1, L(check2)
L(return_at_nul):
    /* temp2 < 0 */
    /* temp2 >= 0 && temp1 < temp2 */
    add str1, str1, temp1
    add str2, str2, temp1
    lbu temp1, 0(str1)
    lbu temp2, 0(str2)
    sub result, temp1, temp2
    ret

    /* temp1 < 0 */
    /* temp2 >= 0 */
L(check2):
    add str1, str1, temp2
    add str2, str2, temp2
    lbu temp1, 0(str1)
    lbu temp2, 0(str2)
    sub result, temp1, temp2
    ret
.option pop
END (STRCMP)
libc_hidden_builtin_def (strcmp)
