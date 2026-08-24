/* Regression: inc/dec PRESERVE CF, so they are not full RFLAGS defs. Here the
   instrumented incq sits between the cmpq that sets CF (unsigned p < q) and the
   jb that consumes it: the inserted access check's test must be bracketed by
   pushfq/popfq, otherwise it clears CF and jb reads the clobbered flags (with
   the bug, jb was never taken). */
	.text
	.globl	incjb
	.type	incjb, @function
incjb:                          ;! long(ptr, ptr)
	cmpq	%rdi, %rsi
	incq	(%rdi)
	jb	.Ltaken
	movq	(%rdi), %rax
	ret
.Ltaken:
	movq	(%rdi), %rax
	addq	$100, %rax
	ret
	.size	incjb, .-incjb
	.section	.note.GNU-stack,"",@progbits
