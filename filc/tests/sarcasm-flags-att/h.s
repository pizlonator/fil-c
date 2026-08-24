	.text
	.globl	h
	.type	h, @function
h:                              ;! long(long, long, ptr)
	cmpq	%rdi, %rsi
	movq	(%rdx), %rax
	leaq	5(%rax), %rcx
	cmoveq	%rcx, %rax
	ret
	.size	h, .-h
	.section	.note.GNU-stack,"",@progbits
