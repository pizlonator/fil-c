/* RISC-V RVV based memcpy.
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

#ifndef MEMCPY
# define MEMCPY memcpy
#endif

#define dst a0
#define src a1
#define num a2

#define ivl a3
#define dst_ptr a4

#define ELEM_LMUL_SETTING m8
#define vdata v0

ENTRY (MEMCPY)
.option push
.option arch, +v
    mv dst_ptr, dst
L(loop):
    vsetvli ivl, num, e8, ELEM_LMUL_SETTING, ta, ma

    vle8.v vdata, (src)
    sub num, num, ivl
    add src, src, ivl
    vse8.v vdata, (dst_ptr)
    add dst_ptr, dst_ptr, ivl

    bnez num, L(loop)

    ret
.option pop
END (MEMCPY)
libc_hidden_builtin_def (memcpy)
