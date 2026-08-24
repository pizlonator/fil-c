/* A branch to a non-local label is a tail call: even with a ;! signature it is
   rejected (tail calls are not yet supported). Branches to local labels are fine. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	b	somefunc            ;! int(ptr)
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

