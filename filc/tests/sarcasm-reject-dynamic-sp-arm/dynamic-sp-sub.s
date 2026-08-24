/* A dynamic (register) sp adjustment is only allowed for an annotated alloca;
   without ';! alloca size' the frame size is unprovable: rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	sub	sp, sp, x0
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

