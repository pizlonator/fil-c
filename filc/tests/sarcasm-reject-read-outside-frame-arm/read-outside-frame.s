/* A load past the end of the frame reads the caller's argument area: sp-relative
   reads, like writes, must stay inside the modeled frame. */
	.text
	.global	f
	.type	f, %function
f:                              ;! unsigned(void)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	ldr	x0, [sp, 64]
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f
