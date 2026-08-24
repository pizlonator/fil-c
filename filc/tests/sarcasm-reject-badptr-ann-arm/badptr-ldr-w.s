/* A 32-bit load cannot load a capability: the destination must be a 64-bit
   GPR. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	ldr	w0, [x1]            ;! load ptr
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

