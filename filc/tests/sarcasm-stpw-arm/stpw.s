	.text
	.globl	stpw_second
	.type	stpw_second, %function
stpw_second:                    ;! long(long, long)
	sub	sp, sp, #16
	stp	w0, w1, [sp]
	ldr	w0, [sp, #4]
	add	sp, sp, #16
	ret
	.size	stpw_second, .-stpw_second
	.globl	stpw_both
	.type	stpw_both, %function
stpw_both:                      ;! long(long, long)
	sub	sp, sp, #16
	stp	w0, w1, [sp]
	ldr	w0, [sp]
	ldr	w1, [sp, #4]
	add	w0, w0, w1
	add	sp, sp, #16
	ret
	.size	stpw_both, .-stpw_both
	.globl	stpw_sum
	.type	stpw_sum, %function
stpw_sum:                       ;! long(long, long)
	sub	sp, sp, #8
	stp	w0, w1, [sp]
	ldp	w0, w1, [sp]
	add	w0, w0, w1
	add	sp, sp, #8
	ret
	.size	stpw_sum, .-stpw_sum
	.section	.note.GNU-stack,"",@progbits
