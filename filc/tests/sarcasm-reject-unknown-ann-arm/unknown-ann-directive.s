/* A ;! annotation on a directive has no meaning and is rejected (annotations go
   on instructions, the function-entry label, or a callsite). */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	.p2align	3              ;! load ptr
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

