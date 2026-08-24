/* An indirect jump through a raw register (computed goto / a switch jump table)
   has no provable target and is rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	br	x9
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

