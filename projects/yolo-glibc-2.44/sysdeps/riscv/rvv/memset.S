/* RISC-V RVV based memset.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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
   <http://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#include <sys/asm.h>

#ifndef MEMSET
# define MEMSET memset
#endif

#define dst a0
#define value a1
#define num a2

#define ivl a3
#define dst_ptr a5

#define ELEM_LMUL_SETTING m8
#define vdata v0

ENTRY (MEMSET)
.option push
.option arch, +v
    mv dst_ptr, dst

    vsetvli ivl, num, e8, ELEM_LMUL_SETTING, ta, ma
    vmv.v.x vdata, value
L(loop):
    vse8.v vdata, (dst_ptr)
    sub num, num, ivl
    add dst_ptr, dst_ptr, ivl
    vsetvli ivl, num, e8, ELEM_LMUL_SETTING, ta, ma
    bnez num, L(loop)

    ret
.option pop
END (MEMSET)
libc_hidden_builtin_def (memset)
