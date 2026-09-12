/* RISC-V RVV based strrchr.
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

#ifndef STRRCHR
# define STRRCHR strrchr
#endif

#define str a0
#define ch a1
#define blk_base a2
#define loc_max a3
#define latest_ptr a4
#define first_idx a5
#define len_valid a5
#define cur_vl a6

#define vhit v0
#define vstr v4
#define vmask_end v8
#define vmask_ch v10
#define vvalid v12
#define vidx v16
#define vacc v20

#if __riscv_xlen == 64
# define XLEN_SHIFT 63
#else
# define XLEN_SHIFT 31
#endif

ENTRY (STRRCHR)
.option push
.option arch, +v
	beqz ch, L(search_zero)
	mv latest_ptr, zero
L(loop):
    mv blk_base, str

    vsetvli cur_vl, zero, e8, m2, ta, ma
    vle8ff.v vstr, (str)

    vmseq.vi vmask_end, vstr, 0
    vmsbf.m vvalid, vmask_end
    vmseq.vx vmask_ch, vstr, ch
    vmand.mm vhit, vmask_ch, vvalid

    vfirst.m first_idx, vhit
    csrr cur_vl, vl
    bltz first_idx, L(no_hit_in_block)

    vsetvli zero, cur_vl, e16, m4, ta, ma
    vid.v vidx
    vmv.s.x vacc, x0
    /* Compute per-block last hit index (or -1 if none). */
    vredmaxu.vs vacc, vidx, vacc, vhit.t
    vmv.x.s loc_max, vacc
    j L(loc_max_ready)

L(no_hit_in_block):
    li loc_max, -1

L(loc_max_ready):
    vsetvli zero, cur_vl, e8, m2, ta, ma
    vcpop.m len_valid, vvalid

    bne len_valid, cur_vl, L(tail_block)
    bltz loc_max, L(advance)

    add latest_ptr, blk_base, loc_max

L(advance):
    add str, str, cur_vl
    j L(loop)

L(tail_block):
    add str, blk_base, loc_max

    /* sign = -1 if loc_max < 0 else 0; then invert to
    build selection mask */
    srai loc_max, loc_max, XLEN_SHIFT
    and len_valid, loc_max, latest_ptr
    xori loc_max, loc_max, -1
    and str, loc_max, str
    or str, str, len_valid
    ret

L(search_zero):
L(sz_loop):
    vsetvli cur_vl, zero, e8, m4, ta, ma
    vle8ff.v vstr, (str)

    vmseq.vi vmask_end, vstr, 0
    vfirst.m first_idx, vmask_end
    bltz first_idx, L(sz_advance)
    add str, str, first_idx
    ret
L(sz_advance):
    csrr cur_vl, vl
    add str, str, cur_vl
    j L(sz_loop)

.option pop
END (STRRCHR)
weak_alias (STRRCHR, rindex)
libc_hidden_builtin_def (strrchr)
