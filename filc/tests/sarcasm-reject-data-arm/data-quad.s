/* A .quad inside a function body (a literal pool or jump table) emits data that
   cannot be given a capability automatically: rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	ldr	x0, [x0]
	.quad	0x1234567890abcdef
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

