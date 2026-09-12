/* RISC-V RVV based strlen.
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

#ifndef STRLEN
# define STRLEN __strlen
#endif

#define result a0
#define str a0
#define copy_str a1
#define ivl a2
#define cur_vl a2
#define end_offset a3

#define ELEM_LMUL_SETTING m2
#define vstr v0
#define vmask_end v2

ENTRY (STRLEN)
.option push
.option arch, +v
    mv copy_str, str
L(loop):
    vsetvli ivl, zero, e8, ELEM_LMUL_SETTING, ta, ma
    vle8ff.v vstr, (copy_str)
    csrr cur_vl, vl
    vmseq.vi vmask_end, vstr, 0
    vfirst.m end_offset, vmask_end
    add copy_str, copy_str, cur_vl
    bltz end_offset, L(loop)

    add str, str, cur_vl
    add copy_str, copy_str, end_offset
    sub result, copy_str, result

    ret
.option pop
END (STRLEN)
weak_alias (STRLEN, strlen)
libc_hidden_builtin_def (strlen)
