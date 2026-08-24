	.text
	.globl	copy16
	.type	copy16, %function
copy16:                         ;! void(ptr, ptr)
	ldr	q0, [x1]
	str	q0, [x0]
	ret
	.size	copy16, .-copy16
	.globl	copyscalar
	.type	copyscalar, %function
copyscalar:                     ;! void(ptr, ptr)
	ldr	s0, [x1]
	str	s0, [x0]
	ldr	h0, [x1, #4]
	str	h0, [x0, #4]
	ldr	b0, [x1, #6]
	str	b0, [x0, #6]
	ldr	b0, [x1, #7]
	str	b0, [x0, #7]
	ret
	.size	copyscalar, .-copyscalar
	.globl	copypair
	.type	copypair, %function
copypair:                       ;! void(ptr, ptr)
	ldp	q0, q1, [x1]
	stp	q0, q1, [x0]
	ret
	.size	copypair, .-copypair
	.section	.note.GNU-stack,"",@progbits
