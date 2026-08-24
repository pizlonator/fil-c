/* fccmp is a conditional FP compare: it reads the condition from NZCV (here the
   eq from the program's cmp) and then writes NZCV. It is classified as BOTH a
   flag use and a flag def (like ccmp/ccmn), so the instrumented ldr between the
   cmp and the fccmp is bracketed by mrs/msr nzcv and the fccmp observes the
   cmp's EQ. With x == 5 the eq holds, so fccmp performs fcmp(d0, d1) with
   d0 == d1 (both fmov'd from x) and the csel takes the eq arm: f(p, 5) == 1,
   f(p, 7) == 0.
   NOTE: this test pins fccmp ACCEPTANCE/passthrough only. Its flag bracket is
   structurally guaranteed regardless of fccmp's own FLAG_USE entry: any
   consumer of fccmp's NZCV output (here the csel) is itself a flagUse, so
   removing fccmp/fccmpe from FLAG_USE/FLAG_DEF produces byte-identical output.
   See sarcasm-adcs-arm for the load-bearing flag-liveness test of this
   bracket (adcs/sbcs, whose carry read is NOT covered by any downstream
   consumer's flagUse). */
	.text
	.global	f
	.type	f, %function
f:                              ;! long(ptr, long)
	cmp	x1, #5
	ldr	x2, [x0]
	fmov	d0, x1
	fmov	d1, x1
	fccmp	d0, d1, #0, eq
	mov	x3, #1
	mov	x4, #0
	csel	x0, x3, x4, eq
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
