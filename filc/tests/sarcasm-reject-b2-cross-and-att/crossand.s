# Multi-clone alias-across-`and` (fail-closed): the jumper parks a carrier
# in its header and B2-joins TWO tails (two clones). One clone is clean; the
# other executes a mid-function `and $-32, %rsp` and then reads through the
# header-parked carrier. Pre- and post-and traffic share one numbering while
# the and shifts rsp by a dynamic slack, so the same slot key names different
# addresses on the two sides — a static error. This pins the
# successors()/x86_preAnd reachability end to end: the header reaches the
# clone's and only through the jumper->clone jump edge, and the in-clone use
# after the and must still be rejected (a missing edge would leave the clone
# unreached and skip its checks, reopening the hole).
	.text
	.globl	xand_jump
	.type	xand_jump, @function
xand_jump:                      ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rsp, %rbx
	movq	%rdi, 0(%rsp)
	movq	%rsi, 8(%rsp)
	testl	%edi, %edi
	jnz	.Lclean_tail
	jmp	.Land_tail
	.size	xand_jump, .-xand_jump
	.globl	xand_and_owner
	.type	xand_and_owner, @function
xand_and_owner:                 ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rsp, %rbx
	movq	%rdi, 0(%rsp)
	nop
.Land_tail:
	and	$-32, %rsp
	movq	8(%rbx), %rax
	addq	0(%rbx), %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	xand_and_owner, .-xand_and_owner
	.globl	xand_clean_owner
	.type	xand_clean_owner, @function
xand_clean_owner:               ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rax
	addq	%rsi, %rax
	nop
.Lclean_tail:
	addq	$3, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	xand_clean_owner, .-xand_clean_owner
	.section	.note.GNU-stack,"",@progbits
