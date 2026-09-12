# Clone-ret static-imbalance at a GOOD entry (fail-closed): the jumper is
# frameless, so the B2 jump fires at the provable entry depth 0 — no
# perturbation, no merge, nothing unprovable about the entry — and the clone
# performs a `sub $16,%rsp` scratch allocation with no matching teardown, so
# its `ret` sits at 16. The clone-balance check's depth-equality branch
# rejects it anyway: a clean entry never excuses an imbalanced return.
# Companion to sarcasm-reject-b2-imbal-att (imbalanced clone at a perturbed
# entry depth 8) and sarcasm-reject-b2-dyndepth-att (unprovable entry depth):
# together they cover imbalance at every entry shape.
	.text
	.globl	igood_jump
	.type	igood_jump, @function
igood_jump:                      ;! long(long)
	movq	%rdi, %rax
	jmp	.Ligood_tail
	.size	igood_jump, .-igood_jump
	.globl	igood_owner
	.type	igood_owner, @function
igood_owner:                     ;! long(long)
	movq	%rdi, %rax
	ret
.Ligood_tail:
	subq	$16, %rsp
	movq	%rax, 0(%rsp)
	addq	$1, %rax
	ret
	.size	igood_owner, .-igood_owner
	.section	.note.GNU-stack,"",@progbits
