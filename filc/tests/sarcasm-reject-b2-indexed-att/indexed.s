# Indexed stack traffic in a B2 clone (fail-closed): only plain base+disp
# slots virtualize — an indexed rsp-relative access keys no slot, in the
# jumper or in any clone, so it is a static error.
	.text
	.globl	idx_jump
	.type	idx_jump, @function
idx_jump:                       ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	jmp	.Lidx_tail
	.size	idx_jump, .-idx_jump
	.globl	idx_owner
	.type	idx_owner, @function
idx_owner:                      ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	nop
.Lidx_tail:
	movq	%rsi, %rax
	movq	0(%rsp,%rax,8), %rbx
	movq	%rbx, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	idx_owner, .-idx_owner
	.section	.note.GNU-stack,"",@progbits
