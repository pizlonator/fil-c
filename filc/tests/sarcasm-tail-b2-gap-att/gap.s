# B2 join across label/nop gaps (the nextInsn/labelAt skip proof, executing):
# the clone entry is buried under extra labels and nops, and the jumper
# header has straight-line nops between its slot traffic and the jump. Both
# maps skip every non-insn statement — labels accumulate to the next insn,
# nextInsn threads insn to insn — so the jumper->clone jump edge still lands
# on the tail's first real instruction and straight-line fallthrough never
# breaks. Executes through the join to prove the edges carried the right
# state (a misresolved edge would misaddress the banded slots and return
# garbage). Companion to sarcasm-reject-b2-preand-gap-att, which pins the
# same skipping for the x86_preAnd reachability proof in the reject
# direction.
	.text
	.globl	gap_jump
	.type	gap_jump, @function
gap_jump:                       ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	nop
	nop
	jmp	.Lgap_tail
	.size	gap_jump, .-gap_jump
	.globl	gap_owner
	.type	gap_owner, @function
gap_owner:                      ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	nop
.Lgap_tail:
.Lgap_alias1:
.Lgap_alias2:
	nop
	nop
	movq	%rbx, 8(%rsp)
	movq	8(%rsp), %rax
	addq	%rsi, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	gap_owner, .-gap_owner
	.section	.note.GNU-stack,"",@progbits
