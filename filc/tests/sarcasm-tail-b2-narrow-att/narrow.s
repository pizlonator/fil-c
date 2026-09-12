# B2 join with sub-word stack traffic in the clone (the frame-extension
# width path): the shared tail spills bytes and words to its inner frame.
# The banded frame extension covers each access's own width (byte/word —
# not a blanket 64), so the synthesized frame backs exactly what the clone
# touches. Executes through the join to prove narrow banded slots address
# the right bytes.
	.text
	.globl	narr_jump
	.type	narr_jump, @function
narr_jump:                      ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	jmp	.Lnarr_tail
	.size	narr_jump, .-narr_jump
	.globl	narr_owner
	.type	narr_owner, @function
narr_owner:                     ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	nop
.Lnarr_tail:
	movb	%sil, 0(%rsp)
	movw	%si, 2(%rsp)
	movq	%rbx, 8(%rsp)
	movzbq	0(%rsp), %rax
	movzwq	2(%rsp), %rcx
	addq	%rcx, %rax
	addq	8(%rsp), %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	narr_owner, .-narr_owner
	.section	.note.GNU-stack,"",@progbits
