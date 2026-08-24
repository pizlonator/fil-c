	.text
	.globl	wrapper
	.type	wrapper, %function
wrapper:                        ;! long(long)
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	bl	maythrow        ;! long(long)
	add	x0, x0, #1
	ldp	x29, x30, [sp], #16
	ret
	.size	wrapper, .-wrapper
	.section	.note.GNU-stack,"",@progbits
