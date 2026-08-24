/* A shift/extend inside the brackets only makes sense with a register index;
   `[x0, lsl #3]` has a shift but no index register, so the effective address
   cannot be modeled for the access check and is rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! long(ptr)
	ldr	x2, [x0, lsl #3]
	mov	x0, x2
	ret
	.size	f, .-f
