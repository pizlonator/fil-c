/* A post-indexed load bumps sp outside the modeled prologue/epilogue forms:
   rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	ldr	x0, [sp], #16
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

