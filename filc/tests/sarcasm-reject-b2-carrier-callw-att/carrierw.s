# Caller-saved clone carrier across a `callw` (fail-closed): `callw` is the
# encodable 16-bit operand-size call — still a hardware call that clobbers
# every caller-saved register. baseMnemonic normalizes it to "call", so the
# frame pass poisons the live caller-saved carrier across it and the later
# recovery revives no provable depth — the clone's `ret` is statically
# rejected. (Without the normalization the call would misclassify and the
# carrier would sail through unsoundly.)
	.text
	.globl	ccw_jump
	.type	ccw_jump, @function
ccw_jump:                       ;! long(long)
	movq	%rdi, %rbx
	jmp	.Lccw_tail
	.size	ccw_jump, .-ccw_jump
	.globl	ccw_owner
	.type	ccw_owner, @function
ccw_owner:                      ;! long(long)
	movq	%rdi, %rbx
	nop
.Lccw_tail:
	movq	%rsp, %rax
	movq	%rbx, %rdi
	callw	ccw_helper        ;! long(long)
	movq	%rax, %rsp
	movq	%rbx, %rax
	ret
	.size	ccw_owner, .-ccw_owner
	.globl	ccw_helper
	.type	ccw_helper, @function
ccw_helper:                     ;! long(long)
	movq	%rdi, %rax
	ret
	.size	ccw_helper, .-ccw_helper
	.section	.note.GNU-stack,"",@progbits
