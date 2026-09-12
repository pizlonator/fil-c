# B2 join with frame-pointer-relative traffic in the clone (the shared-fp
# shape): the jumper establishes %rbp and spills its argument to an
# rbp-relative slot, then joins its sibling's tail mid-body. The clone
# addresses the same slot through %rbp — rbp-relative accesses key off the
# shared frame pointer (not the clone band), so both spellings unify and the
# join executes with the jumper's frame. The leave-based epilogue is carried
# in the region and returns at the entry rsp through a verified teardown
# span.
	.text
	.globl	rbp_jump
	.type	rbp_jump, @function
rbp_jump:                       ;! long(long,long)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$32, %rsp
	movq	%rdi, -8(%rbp)
	movq	%rsi, %rcx
	jmp	.Lrbp_tail
	.size	rbp_jump, .-rbp_jump
	.globl	rbp_owner
	.type	rbp_owner, @function
rbp_owner:                      ;! long(long,long)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$32, %rsp
	movq	%rdi, -8(%rbp)
	movq	%rsi, %rcx
	nop
.Lrbp_tail:
	movq	-8(%rbp), %rax
	addq	%rcx, %rax
	addq	$7, %rax
	addq	$32, %rsp
	leave
	ret
	.size	rbp_owner, .-rbp_owner
	.section	.note.GNU-stack,"",@progbits
