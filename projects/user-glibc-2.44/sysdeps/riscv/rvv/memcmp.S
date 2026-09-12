/* RISC-V RVV based memcmp.
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

#ifndef MEMCMP
# define MEMCMP memcmp
#endif

#define result a0

#define src1 a0
#define src2 a1
#define num a2

#define ivl a3
#define temp a4

#define ELEM_LMUL_SETTING m8
#define vdata1 v0
#define vdata2 v8
#define vmask v16
#define vtemp1 v16
#define vtemp2 v24

ENTRY (MEMCMP)
.option push
.option arch, +v
L(loop):
    vsetvli ivl, num, e8, ELEM_LMUL_SETTING, ta, ma

    vle8.v vdata1, (src1)
    vle8.v vdata2, (src2)

    vmsne.vv vmask, vdata1, vdata2
    sub num, num, ivl
    vfirst.m temp, vmask

    bgez temp, L(found)

    add src1, src1, ivl
    add src2, src2, ivl

    bnez num, L(loop)

    li result, 0
    ret
L(found):
    vrgather.vx vtemp1, vdata1, temp
    vrgather.vx vtemp2, vdata2, temp
    vmv.x.s result, vtemp1
    vmv.x.s temp, vtemp2
    andi result, result, 0xff
    andi temp, temp, 0xff
    sub result, result, temp
    ret
.option pop
END (MEMCMP)
libc_hidden_builtin_def(memcmp)
#ifdef weak_alias
#undef bcmp
weak_alias (MEMCMP, bcmp)
#endif

#undef __memcmpeq
strong_alias (MEMCMP, __memcmpeq)
libc_hidden_builtin_def (MEMCMP)
libc_hidden_def (__memcmpeq)
