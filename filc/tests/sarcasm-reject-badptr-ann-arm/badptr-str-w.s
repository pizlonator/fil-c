/* A 32-bit store cannot store a capability: the stored value must be a 64-bit
   GPR. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	str	w0, [x1]            ;! store ptr
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

