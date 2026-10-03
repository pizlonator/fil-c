	.text
# Two long forms with the SAME spelling sharing one stack-alias register that
# aliases the stack at different offsets at the different points: the resolved
# ranges disagree, so this is rejected (the "special case").
	.globl	f
	.type	f, @function
f:                              ;! int(size_t)
	subq	$96, %rsp
	movq	%rdi, 64(%rsp)
	leaq	-8(%rsp), %rbx
	movl	(%rbx,%rdi), %eax #! stack buffer (y, %rbx + 8, %rbx + 40)
	leaq	-16(%rsp), %rbx
	movl	(%rbx,%rdi), %ebx #! stack buffer (y, %rbx + 8, %rbx + 40)
	addq	$96, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
