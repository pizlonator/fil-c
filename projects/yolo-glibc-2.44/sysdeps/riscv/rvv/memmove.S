/* RISC-V RVV based memmove.
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

#ifndef MEMMOVE
# define MEMMOVE memmove
#endif

#define dst a0
#define src a1
#define num a2

#define ivl a3
#define dst_ptr a4
#define diff a5

#define ELEM_LMUL_SETTING m8
#define vdata v0

ENTRY (MEMMOVE)
.option push
.option arch, +v
    /* There is nothing to do if we are copying zero bytes or DST == SRC.  */
    beqz num, L(done)
    sub diff, dst, src
    beqz diff, L(done)

    /* If (unsigned)(dst - src) < num, then we have an overlap scenario that
       requires a backward copy loop.  Otherwise, we can use a faster forward
       copy loop.  */
    bltu diff, num, L(do_backward_loop)

    mv dst_ptr, dst

L(forward_copy_loop):
    vsetvli ivl, num, e8, ELEM_LMUL_SETTING, ta, ma
    vle8.v vdata, (src)
    sub num, num, ivl
    add src, src, ivl
    vse8.v vdata, (dst_ptr)
    add dst_ptr, dst_ptr, ivl
    bnez num, L(forward_copy_loop)
L(done):
    ret

L(do_backward_loop):
    add src, src, num
    add dst_ptr, dst, num

L(backward_copy_loop):
    vsetvli ivl, num, e8, ELEM_LMUL_SETTING, ta, ma
    sub src, src, ivl
    sub dst_ptr, dst_ptr, ivl
    vle8.v vdata, (src)
    sub num, num, ivl
    vse8.v vdata, (dst_ptr)
    bnez num, L(backward_copy_loop)
    ret

.option pop
END (MEMMOVE)
libc_hidden_builtin_def (memmove)
