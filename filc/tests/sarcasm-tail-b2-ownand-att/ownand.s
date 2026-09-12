# B2 clone with its OWN mid-function `and` (the must-not-poison test): the
# shared tail aligns rsp with `and $-32, %rsp` and runs its scratch traffic
# strictly after it. The x86_preAnd mark covers exactly the instructions that
# can REACH the and — the jumper header and the owner's pre-tail code reach
# it only through the jump/fallthrough edge, but neither holds direct
# rsp-relative slot traffic there (registers only), so nothing legal is
# marked and the join compiles. The clone's post-and traffic keys post-and
# numbering and executes correctly.
#
# Contrast sarcasm-reject-b2-cross-and-att (jumper parks a carrier and the
# clone reads through it after its and — rejected): the difference is
# whether pre-and-keyed traffic EXISTS, not whether an `and` exists. A
# clone's own alignment must not poison a clean jumper. Executes through
# the join to prove post-and banded slots address the right bytes.
	.text
	.globl	oan_jump
	.type	oan_jump, @function
oan_jump:                       ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	jmp	.Loan_tail
	.size	oan_jump, .-oan_jump
	.globl	oan_owner
	.type	oan_owner, @function
oan_owner:                      ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	nop
.Loan_tail:
	and	$-32, %rsp
	movq	%rbx, 0(%rsp)
	movq	0(%rsp), %rax
	addq	%rsi, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	oan_owner, .-oan_owner
	.section	.note.GNU-stack,"",@progbits
