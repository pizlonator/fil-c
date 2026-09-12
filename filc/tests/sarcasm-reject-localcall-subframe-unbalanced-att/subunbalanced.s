# A local subroutine's `sub $N,%rsp` scratch frame torn down on no path before
# `ret` is a static error: the clone's frame is dropped (the real rsp never
# moves), so every localRet must sit at exactly its clone entry's depth — the
# continuation resumes at the caller's depth.
	.text
	.globl	subunbalanced_main
	.type	subunbalanced_main, @function
subunbalanced_main:             ;! long(long,long)
	pushq	%rbx
	subq	$32, %rsp
	movq	%rdi, 0(%rsp)
	movq	%rsi, 8(%rsp)
	call	subunbalanced
	movq	0(%rsp), %rax
	addq	8(%rsp), %rax
	addq	$32, %rsp
	popq	%rbx
	ret
	.size	subunbalanced_main, .-subunbalanced_main
	.type	subunbalanced, @function
subunbalanced:
	subq	$72, %rsp
	movq	$100, 0(%rsp)
	movq	$200, 8(%rsp)
	movq	0(%rsp), %rax
	addq	8(%rsp), %rax
	ret
	.size	subunbalanced, .-subunbalanced
	.section	.note.GNU-stack,"",@progbits
