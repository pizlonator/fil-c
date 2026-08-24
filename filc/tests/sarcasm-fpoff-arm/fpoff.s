	.text
	.globl	fpoff
	.type	fpoff, %function
fpoff:                          ;! long(long, long)
	stp	x29, x30, [sp, #-48]!
	add	x29, sp, #16
	str	x0, [x29, #-8]
	str	x1, [x29, #8]
	str	w0, [x29, #20]
	ldr	x2, [x29, #-8]
	ldr	x3, [x29, #8]
	add	x2, x2, x3
	ldr	w4, [x29, #20]
	add	x0, x2, x4
	ldp	x29, x30, [sp], #48
	ret
	.size	fpoff, .-fpoff
	.section	.note.GNU-stack,"",@progbits
