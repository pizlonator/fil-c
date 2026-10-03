# Feature 2 (rsp-relative accesses before a mid-function `and $-N, %rsp`): a
# pre-and rsp-relative access at a provable depth keys TRUE entry-relative
# coordinates — the same slot the equivalent pre-and slot gets without any
# and in the function — so it compiles and runs. The and (dropped, recorded)
# provides the 32-byte alignment for the post-and vector scratch; the pre-and
# slots and the post-and scratch are disjoint modeled ranges, so the two
# coordinate systems never share bytes.
	.text
	.globl	preand
	.type	preand, @function
preand:                         ;! long(long)
	jmp	.Lenter
.Lenter:
	subq	$128, %rsp
	movq	%rdi, 8(%rsp)		# pre-and rsp-relative store (TRUE coords)
	movq	8(%rsp), %rax		# pre-and load
	andq	$-32, %rsp		# mid-function realignment (dropped, recorded)
	movdqa	%xmm0, 64(%rsp)		# post-and vector scratch (and-keyed)
	movdqa	64(%rsp), %xmm1
	addq	$1, %rax		# x + 1
	addq	$128, %rsp
	ret
	.size	preand, .-preand
	.section	.note.GNU-stack,"",@progbits
