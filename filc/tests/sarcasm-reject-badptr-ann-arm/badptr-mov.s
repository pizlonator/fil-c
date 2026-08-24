/* A register-to-register mov has no memory operand to bounds-check: ';! load
   ptr' requires a 64-bit scalar load. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	mov	x0, x1              ;! load ptr
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

