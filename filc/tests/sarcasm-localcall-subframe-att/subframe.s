# A local subroutine with its own stack frame: the clone runs `subq $72,%rsp`,
# spills locals into its scratch area (perturbed-depth slots keyed exactly),
# reads the caller's frame through the +8 rule (`8(%rsp)` is the caller's
# `0(%rsp)` slot), and tears the frame down before returning.
	.text
	.globl	subframe_main
	.type	subframe_main, @function
subframe_main:                  ;! long(long,long)
	pushq	%rbx
	subq	$32, %rsp
	movq	%rdi, 0(%rsp)
	movq	%rsi, 8(%rsp)
	movq	$7, 16(%rsp)
	call	subscratch
	movq	0(%rsp), %rax
	addq	8(%rsp), %rax
	addq	16(%rsp), %rax
	addq	$32, %rsp
	popq	%rbx
	ret
	.size	subframe_main, .-subframe_main
	.type	subscratch, @function
subscratch:
	subq	$72, %rsp
	movq	$100, 0(%rsp)
	movq	$200, 8(%rsp)
	movq	$300, 16(%rsp)
	movq	16+72+8(%rsp), %rax
	addq	$11, %rax
	movq	%rax, 24(%rsp)
	movq	0(%rsp), %rax
	addq	8(%rsp), %rax
	addq	16(%rsp), %rax
	addq	24(%rsp), %rax
	movq	%rax, 16+72+8(%rsp)
	addq	$72, %rsp
	ret
	.size	subscratch, .-subscratch
	.section	.note.GNU-stack,"",@progbits
