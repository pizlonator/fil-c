/* RISC-V RVV based memchr.
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

#ifndef MEMCHR
# define MEMCHR __memchr
#endif

#define result a0

#define src a0
#define value a1
#define num a2

#define ivl a3
#define temp a4

#define ELEM_LMUL_SETTING m8
#define vdata v0
#define vmask v8

ENTRY (MEMCHR)
.option push
.option arch, +v
L(loop):
    vsetvli zero, num, e8, ELEM_LMUL_SETTING, ta, ma

    vle8ff.v vdata, (src)
    /* Find the value inside the loaded data.  */
    vmseq.vx vmask, vdata, value
    vfirst.m temp, vmask

    /* Skip the loop if we find the matched value.  */
    bgez temp, L(found)

    csrr ivl, vl
    sub num, num, ivl
    add src, src, ivl

    bnez num, L(loop)

    li result, 0
    ret
L(found):
    add result, src, temp
    ret
.option pop
END (MEMCHR)
weak_alias (MEMCHR, memchr)
libc_hidden_builtin_def (memchr)
