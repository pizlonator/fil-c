/* ';! frobnicate' is not a recognized annotation form: rejected (the expected
   forms are 'load ptr', 'store ptr', 'alloca ...' or a callsite signature). */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	mov	x1, x0              ;! frobnicate
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

