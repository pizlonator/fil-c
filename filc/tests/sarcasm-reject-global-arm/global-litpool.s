/* An explicit literal-pool reference loads data from a pool that cannot be made
   memory-safe automatically: rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	ldr	x1, .Lpool
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

