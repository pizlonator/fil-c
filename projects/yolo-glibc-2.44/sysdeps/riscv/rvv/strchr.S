/* RISC-V RVV based strchr.
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

#ifndef STRCHR
# define STRCHR strchr
#endif

#define str a0
#define ch a1
#define end_offset a2
#define ch_offset a3
#define temp1 a4
#define temp2 a5
#define cur_vl a6
#define ivl t0

#define ELEM_LMUL_SETTING m1
#define vstr v0
#define vmask_end v8
#define vmask_ch v9

ENTRY (STRCHR)
.option push
.option arch, +v
L(strchr_loop):
    vsetvli ivl, zero, e8, ELEM_LMUL_SETTING, ta, ma
    vle8ff.v vstr, (str)
    vmseq.vi vmask_end, vstr, 0
    vmseq.vx vmask_ch, vstr, ch
    vfirst.m end_offset, vmask_end /* first occurrence of \0 */
    vfirst.m ch_offset, vmask_ch /* first occurrence of ch */
    sltz temp1, ch_offset
    sltu temp2, end_offset, ch_offset
    or temp1, temp1, temp2
    beqz temp1, L(found_ch) /* Found ch, not preceded by \0? */
    csrr cur_vl, vl
    add str, str, cur_vl
    bltz end_offset, L(strchr_loop) /* Didn't find \0? */
    li str, 0
    ret
L(found_ch):
    add str, str, ch_offset
    ret
.option pop
END (STRCHR)
weak_alias (STRCHR, index)
libc_hidden_builtin_def (strchr)
