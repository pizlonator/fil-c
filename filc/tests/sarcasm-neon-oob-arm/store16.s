	.text
	.globl	store16
	.type	store16, %function
store16:                        ;! void(ptr, long, long)
	fmov	d0, x1
	fmov	v0.d[1], x2
	str	q0, [x0]
	ret
	.size	store16, .-store16
	.section	.note.GNU-stack,"",@progbits
