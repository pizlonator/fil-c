	.text
# A rep movsb whose byte count (%rcx * element) exceeds the annotated buffer's
# range: the dynamic count check must trap before the copy runs.
	.globl	rep_oob
	.type	rep_oob, @function
rep_oob:                        ;! void(ptr, size_t)
	subq	$32, %rsp
	movdqu	(%rsi), %xmm0
	movdqu	%xmm0, (%rsp)
	leaq	(%rsp), %rsi
	movq	%rdx, %rcx
	rep movsb #! stack buffer (t, %rsp, %rsp + 16)
	addq	$32, %rsp
	ret
	.size	rep_oob, .-rep_oob
	.section	.note.GNU-stack,"",@progbits
