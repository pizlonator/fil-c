/* A store at a negative sp offset scribbles below the frame (into the red zone
   / whatever the signal stack has there): rejected like an out-of-frame access. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	str	x0, [sp, -8]
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f
