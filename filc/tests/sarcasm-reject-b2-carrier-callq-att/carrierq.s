# Caller-saved clone carrier across a `callq` (fail-closed): `callq` is the
# same hardware call as `call` (one baseMnemonic), so it clobbers every
# caller-saved register the same way. The clone parks the jump-site rsp in
# %rax, the annotated callq clobbers it, and recovering %rsp through it
# afterwards revives no provable depth — the clone's `ret` is statically
# rejected, exactly like the bare-`call` shape.
	.text
	.globl	ccq_jump
	.type	ccq_jump, @function
ccq_jump:                       ;! long(long)
	movq	%rdi, %rbx
	jmp	.Lccq_tail
	.size	ccq_jump, .-ccq_jump
	.globl	ccq_owner
	.type	ccq_owner, @function
ccq_owner:                      ;! long(long)
	movq	%rdi, %rbx
	nop
.Lccq_tail:
	movq	%rsp, %rax
	movq	%rbx, %rdi
	callq	ccq_helper        ;! long(long)
	movq	%rax, %rsp
	movq	%rbx, %rax
	ret
	.size	ccq_owner, .-ccq_owner
	.globl	ccq_helper
	.type	ccq_helper, @function
ccq_helper:                     ;! long(long)
	movq	%rdi, %rax
	addq	$1, %rax
	ret
	.size	ccq_helper, .-ccq_helper
	.section	.note.GNU-stack,"",@progbits
