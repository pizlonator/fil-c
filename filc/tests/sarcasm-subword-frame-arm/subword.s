	.text
	.globl	subword
	.type	subword, %function
subword:                        ;! long(long)
	stp	x29, x30, [sp, #-32]!
	mov	x29, sp
	strb	w0, [sp, #23]
	strh	w0, [sp, #18]
	ldrsb	x1, [sp, #23]
	ldrb	w2, [sp, #23]
	ldrsh	x3, [sp, #18]
	ldrh	w4, [sp, #18]
	strb	w0, [x29, #21]
	ldrsb	x5, [x29, #21]
	add	x1, x1, x2
	add	x1, x1, x3
	add	x1, x1, x4
	add	x0, x1, x5
	ldp	x29, x30, [sp], #32
	ret
	.size	subword, .-subword
	.globl	mixwidth
	.type	mixwidth, %function
mixwidth:                       ;! long(long)
	stp	x29, x30, [sp, #-32]!
	mov	x29, sp
	str	w0, [sp, #12]
	ldrb	w1, [sp, #12]
	ldrh	w2, [sp, #12]
	ldr	w3, [sp, #12]
	add	w1, w1, w2
	add	w1, w1, w3
	mov	x0, x1
	ldp	x29, x30, [sp], #32
	ret
	.size	mixwidth, .-mixwidth
	.globl	signcheck
	.type	signcheck, %function
signcheck:                      ;! long(long)
	stp	x29, x30, [sp, #-32]!
	mov	x29, sp
	str	w0, [sp, #12]
	ldrsw	x0, [sp, #12]
	ldp	x29, x30, [sp], #32
	ret
	.size	signcheck, .-signcheck
	.globl	fpgcc
	.type	fpgcc, %function
fpgcc:                          ;! long(long)
	stp	x29, x30, [sp, #-32]!
	add	x29, sp, #16
	strh	w0, [x29, #-4]
	ldrsh	x1, [x29, #-4]
	strb	w0, [x29, #3]
	ldrsb	x2, [x29, #3]
	add	x0, x1, x2
	ldp	x29, x30, [sp], #32
	ret
	.size	fpgcc, .-fpgcc
	.section	.note.GNU-stack,"",@progbits
