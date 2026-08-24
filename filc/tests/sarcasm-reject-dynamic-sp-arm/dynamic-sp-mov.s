/* Writing sp from a register replaces the frame base with an unprovable value:
   rejected (only the annotated alloca forms may move sp dynamically). */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	mov	sp, x0
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

