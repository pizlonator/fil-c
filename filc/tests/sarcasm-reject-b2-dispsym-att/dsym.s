# Symbolic-displacement stack traffic in a B2 clone (fail-closed): a
# symbolic displacement has an unknown-at-compile-time target that cannot be
# bounds-checked or virtualized — in the jumper or in any clone — so it is a
# static error.
	.text
	.globl	dsym_jump
	.type	dsym_jump, @function
dsym_jump:                      ;! long(long)
	movq	%rdi, %rbx
	jmp	.Ldsym_tail
	.size	dsym_jump, .-dsym_jump
	.globl	dsym_owner
	.type	dsym_owner, @function
dsym_owner:                     ;! long(long)
	movq	%rdi, %rbx
	nop
.Ldsym_tail:
	movq	foosym(%rsp), %rax
	addq	%rbx, %rax
	ret
	.size	dsym_owner, .-dsym_owner
	.section	.note.GNU-stack,"",@progbits
