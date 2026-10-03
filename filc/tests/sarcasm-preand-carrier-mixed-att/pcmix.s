# Feature 1 + Feature 2 together: a pre-and LEA carrier (parked before the
# and; its value is absolute so the and cannot touch it) mixed with pre-and
# rsp-relative traffic, both spellings addressing one slot. The pre-and slots
# (o = -32 and -56) and the post-and vector scratch (o = -24) occupy disjoint
# modeled ranges, so one function carries both coordinate systems and runs.
	.text
	.globl	pcmix
	.type	pcmix, @function
pcmix:                          ;! long(long)
	jmp	.Lenter
.Lenter:
	subq	$64, %rsp
	leaq	16(%rsp), %rbx		# pre-and carrier: entry_rsp - 48
	movq	%rdi, 16(%rbx)		# [entry-32] = x   (carrier spelling)
	movq	%rdi, 8(%rsp)		# [entry-56] = x   (rsp spelling, pre-and)
	movq	16(%rbx), %rax		# x (carrier spelling)
	movq	8(%rsp), %rcx		# x (rsp spelling)
	addq	%rcx, %rax		# 2x
	andq	$-32, %rsp		# mid-function realignment (dropped, recorded)
	movdqa	%xmm0, 48(%rsp)		# post-and scratch (disjoint range)
	movdqa	48(%rsp), %xmm1
	addq	$64, %rsp
	ret
	.size	pcmix, .-pcmix
	.section	.note.GNU-stack,"",@progbits
