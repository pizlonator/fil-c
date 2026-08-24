	.text
	.globl	sum3
	.type	sum3, %function
sum3:                           ;! long(ptr, ptr, ptr)
	ldr	x3, [x0]
	ldr	x4, [x1]
	ldr	x5, [x2]
	add	x0, x3, x4
	add	x0, x0, x5
	ret
	.size	sum3, .-sum3
	.section	.note.GNU-stack,"",@progbits
