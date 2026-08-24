/* A pair store stores two values; a single capability cannot cover both, so
   ';! store ptr' is only valid on a 64-bit scalar str/stur. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	stp	x0, x1, [x2]        ;! store ptr
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

