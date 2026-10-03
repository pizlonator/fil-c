# Feature 2: pre-and rsp-relative traffic combined with post-and traffic —
# rbp-relative slots (absolute, TRUE coordinates) both before and after the
# and, post-and rsp-relative vector scratch (and-keyed), and pre-and
# rsp-relative GPR slots (TRUE). The pre-and slots and the post-and scratch
# occupy disjoint modeled ranges, so one function carries both coordinate
# systems and runs.
	.text
	.globl	preandmix
	.type	preandmix, @function
preandmix:                      ;! long(long)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$128, %rsp
	movq	%rdi, -16(%rbp)		# rbp-relative (TRUE coords, pre-and)
	movq	%rdi, 8(%rsp)		# pre-and rsp-relative store (TRUE coords)
	movq	8(%rsp), %rax		# pre-and load
	andq	$-32, %rsp		# mid-function realignment (dropped, recorded)
	movdqa	%xmm0, 64(%rsp)		# post-and vector scratch (and-keyed)
	movdqa	64(%rsp), %xmm1
	movq	%rax, -24(%rbp)		# rbp-relative store AFTER the and (absolute)
	movq	-16(%rbp), %rax
	addq	-24(%rbp), %rax		# 2x
	leave
	ret
	.size	preandmix, .-preandmix
	.section	.note.GNU-stack,"",@progbits
