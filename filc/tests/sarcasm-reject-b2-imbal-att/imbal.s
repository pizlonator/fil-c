# Clone-ret static-imbalance rejection (fail-closed): the clone performs a
# `sub $16,%rsp` scratch allocation with no matching teardown on its (only)
# path, so its `ret` sits 16 below the clone-entry depth. The clone-balance
# check's depth-equality branch rejects it. Companion to
# sarcasm-reject-b2-dyndepth-att, which pins the unknown ("dyn") depth
# branch: together they cover every unbalanced return. The shared tail sits
# after the owner's own balanced `ret`, so the owner stays clean and the
# error names the clone's `ret` specifically (dead in the owner, live in the
# jumper's clone).
	.text
	.globl	imbal_jump
	.type	imbal_jump, @function
imbal_jump:                      ;! long(long)
	subq	$8, %rsp
	movq	%rdi, %rax
	jmp	.Limbal_tail
	.size	imbal_jump, .-imbal_jump
	.globl	imbal_owner
	.type	imbal_owner, @function
imbal_owner:                      ;! long(long)
	movq	%rdi, %rax
	ret
.Limbal_tail:
	subq	$16, %rsp
	movq	%rax, 0(%rsp)
	addq	$1, %rax
	ret
	.size	imbal_owner, .-imbal_owner
	.section	.note.GNU-stack,"",@progbits
