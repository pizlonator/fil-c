/* A pair load loads two values; a single capability cannot cover both, so
   ';! load ptr' is only valid on a 64-bit scalar ldr/ldur. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	ldp	x0, x1, [x2]        ;! load ptr
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

