/* A load into xzr discards the value: the destination is not a real 64-bit GPR,
   so no capability can be delivered. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	ldr	xzr, [x0]           ;! load ptr
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

