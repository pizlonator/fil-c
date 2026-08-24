/* An indirect call through a raw register has no provable target: only calls
   through function-pointer values (which carry capabilities) can be made safe. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	blr	x9
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

