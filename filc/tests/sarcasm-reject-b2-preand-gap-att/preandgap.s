# Alias-across-`and` through a label/nop gap (fail-closed): the clone entry
# is separated from the mid-function `and $-32, %rsp` by extra labels and
# nops, and the clone then reads through the jumper-header-parked carrier.
# Pre- and post-and traffic share one numbering while the and shifts rsp by
# a dynamic slack, so the same slot key names different addresses — a static
# error. The gap must not break the reachability the x86_preAnd proof rides
# on: labelAt/nextInsn skip every non-insn, so the jumper->clone jump edge
# still lands on the `and` and the header stays marked pre-and. (Without the
# edge the clone would analyze unreached and skip its checks, reopening the
# hole.) Companion to the gapless multi-clone
# sarcasm-reject-b2-cross-and-att.
	.text
	.globl	pgap_jump
	.type	pgap_jump, @function
pgap_jump:                      ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rsp, %rbx
	movq	%rdi, 0(%rsp)
	movq	%rsi, 8(%rsp)
	jmp	.Lpgap_tail
	.size	pgap_jump, .-pgap_jump
	.globl	pgap_owner
	.type	pgap_owner, @function
pgap_owner:                     ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rsp, %rbx
	movq	%rdi, 0(%rsp)
	nop
.Lpgap_tail:
.Lpgap_alias1:
.Lpgap_alias2:
	nop
	nop
	and	$-32, %rsp
	movq	8(%rbx), %rax
	addq	0(%rbx), %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	pgap_owner, .-pgap_owner
	.section	.note.GNU-stack,"",@progbits
