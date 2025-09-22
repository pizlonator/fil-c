dnl  ARM64 mpn_divrem_1 and mpn_preinv_divrem_1.

dnl  Contributed to the GNU project by Torbjörn Granlund.

dnl  Copyright 2020 Free Software Foundation, Inc.

dnl  This file is part of the GNU MP Library.
dnl
dnl  The GNU MP Library is free software; you can redistribute it and/or modify
dnl  it under the terms of either:
dnl
dnl    * the GNU Lesser General Public License as published by the Free
dnl      Software Foundation; either version 3 of the License, or (at your
dnl      option) any later version.
dnl
dnl  or
dnl
dnl    * the GNU General Public License as published by the Free Software
dnl      Foundation; either version 2 of the License, or (at your option) any
dnl      later version.
dnl
dnl  or both in parallel, as here.
dnl
dnl  The GNU MP Library is distributed in the hope that it will be useful, but
dnl  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
dnl  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
dnl  for more details.
dnl
dnl  You should have received copies of the GNU General Public License and the
dnl  GNU Lesser General Public License along with the GNU MP Library.  If not,
dnl  see https://www.gnu.org/licenses/.

include(`../config.m4')

dnl TODO
dnl  * Handle the most significant quotient limb for the unnormalised case
dnl    specially, just like in the C code.  (It is very often 0.)

define(`qp_arg',   x0)
define(`fn_arg',   x1)
define(`np_arg',   x2)
define(`n_arg',    x3)
define(`d_arg',    x4)
define(`dinv_arg', x5)
define(`cnt_arg',  x6)

define(`qp',   x19)
define(`np',   x20)
define(`n',    x21)
define(`d',    x22)
define(`fn',   x24)
define(`dinv', x0)
define(`cnt',  x23)
define(`tnc',  x8)

dnl mp_limb_t
dnl mpn_divrem_1 (mp_ptr qp, mp_size_t fn,
dnl               mp_srcptr np, mp_size_t n,
dnl               mp_limb_t d_unnorm)

dnl mp_limb_t
dnl mpn_preinv_divrem_1 (mp_ptr qp, mp_size_t fn,
dnl                      mp_srcptr np, mp_size_t n,
dnl                      mp_limb_t d_unnorm, mp_limb_t dinv, int cnt)

ASM_START()

PROLOGUE(mpn_preinv_divrem_1)
	cbz	n_arg, L(fz)
	stp	x29, x30, [sp, #-80]!
	mov	x29, sp
	stp	x19, x20, [sp, #16]
	stp	x21, x22, [sp, #32]
	stp	x23, x24, [sp, #48]

	sub	n, n_arg, #1
	add	x7, n, fn_arg
	add	np, np_arg, n, lsl #3
	add	qp, qp_arg, x7, lsl #3
	mov	fn, fn_arg
	mov	d, d_arg
	mov	dinv, dinv_arg
	tbnz	d_arg, #63, L(nentry)
	mov	cnt, cnt_arg
	b	L(uentry)
EPILOGUE()

PROLOGUE(mpn_divrem_1)
	cbz	n_arg, L(fz)
	stp	x29, x30, [sp, #-80]!
	mov	x29, sp
	stp	x19, x20, [sp, #16]
	stp	x21, x22, [sp, #32]
	stp	x23, x24, [sp, #48]

	sub	n, n_arg, #1
	add	x7, n, fn_arg
	add	np, np_arg, n, lsl #3
	add	qp, qp_arg, x7, lsl #3
	mov	fn, fn_arg
	mov	d, d_arg
	tbnz	d_arg, #63, L(normalised)

L(unnorm):
	clz	cnt, d
	lsl	x0, d, cnt
	bl	GSYM_PREFIX`'MPN(invert_limb)
L(uentry):
	lsl	d, d, cnt
	ldr	x7, [np], #-8
	sub	tnc, xzr, cnt
	lsr	x11, x7, tnc		C r
	lsl	x1, x7, cnt
	cbz	n, L(uend)

L(utop):ldr	x7, [np], #-8
	add	x2, x11, #1
	mul	x10, x11, dinv
	umulh	x17, x11, dinv
	lsr	x9, x7, tnc
	orr	x1, x1, x9
	adds	x10, x1, x10
	adc	x2, x2, x17
	msub	x11, d, x2, x1
	lsl	x1, x7, cnt
	cmp	x10, x11
	add	x14, x11, d
	csel	x11, x14, x11, cc
	sbc	x2, x2, xzr
	cmp	x11, d
	bcs	L(ufx)
L(uok):	str	x2, [qp], #-8
	sub	n, n, #1
	cbnz	n, L(utop)

L(uend):add	x2, x11, #1
	mul	x10, x11, dinv
	umulh	x17, x11, dinv
	adds	x10, x1, x10
	adc	x2, x2, x17
	msub	x11, d, x2, x1
	cmp	x10, x11
	add	x14, x11, d
	csel	x11, x14, x11, cc
	sbc	x2, x2, xzr
	subs	x14, x11, d
	adc	x2, x2, xzr
	csel	x11, x14, x11, cs
	str	x2, [qp], #-8

	cbnz	fn, L(ftop)
	lsr	x0, x11, cnt
	ldp	x19, x20, [sp, #16]
	ldp	x21, x22, [sp, #32]
	ldp	x23, x24, [sp, #48]
	ldp	x29, x30, [sp], #80
	ret

L(ufx):	add	x2, x2, #1
	sub	x11, x11, d
	b	L(uok)


L(normalised):
	mov	x0, d
	bl	GSYM_PREFIX`'MPN(invert_limb)
L(nentry):
	ldr	x7, [np], #-8
	subs	x14, x7, d
	adc	x2, xzr, xzr		C hi q limb
	csel	x11, x14, x7, cs
	b	L(nok)

L(ntop):ldr	x1, [np], #-8
	add	x2, x11, #1
	mul	x10, x11, dinv
	umulh	x17, x11, dinv
	adds	x10, x1, x10
	adc	x2, x2, x17
	msub	x11, d, x2, x1
	cmp	x10, x11
	add	x14, x11, d
	csel	x11, x14, x11, cc	C remainder
	sbc	x2, x2, xzr
	cmp	x11, d
	bcs	L(nfx)
L(nok):	str	x2, [qp], #-8
	sub	n, n, #1
	tbz	n, #63, L(ntop)

L(nend):cbnz	fn, L(frac)
	mov	x0, x11
	ldp	x19, x20, [sp, #16]
	ldp	x21, x22, [sp, #32]
	ldp	x23, x24, [sp, #48]
	ldp	x29, x30, [sp], #80
	ret

L(nfx):	add	x2, x2, #1
	sub	x11, x11, d
	b	L(nok)

L(frac):mov	cnt, #0
L(ftop):add	x2, x11, #1
	mul	x10, x11, dinv
	umulh	x17, x11, dinv
	add	x2, x2, x17
	msub	x11, d, x2, xzr
	cmp	x10, x11
	add	x14, x11, d
	csel	x11, x14, x11, cc	C remainder
	sbc	x2, x2, xzr
	str	x2, [qp], #-8
	sub	fn, fn, #1
	cbnz	fn, L(ftop)

	lsr	x0, x11, cnt
	ldp	x19, x20, [sp, #16]
	ldp	x21, x22, [sp, #32]
	ldp	x23, x24, [sp, #48]
	ldp	x29, x30, [sp], #80
	ret

C Block zero. We need this for the degenerated case of n = 0, fn != 0.
L(fz):	cbz	fn_arg, L(zend)
L(ztop):str	xzr, [qp_arg], #8
	sub	fn_arg, fn_arg, #1
	cbnz	fn_arg, L(ztop)
L(zend):mov	x0, #0
	ret
EPILOGUE()
