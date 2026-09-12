# Escaping clone carrier (fail-closed): the clone parks the jump-site rsp
# in callee-saved %rbx (a legal carrier save) and then stores the register
# itself to caller memory. The parked value is phantom — it has no output
# existence — so any read but the carrier discipline's is a static error.
	.text
	.globl	cesc_jump
	.type	cesc_jump, @function
cesc_jump:                      ;! long(ptr,long)
	pushq	%rbx
	pushq	%r12
	subq	$64, %rsp
	movq	%rdi, %r12
	jmp	.Lcesc_tail
	.size	cesc_jump, .-cesc_jump
	.globl	cesc_owner
	.type	cesc_owner, @function
cesc_owner:                     ;! long(ptr,long)
	pushq	%rbx
	pushq	%r12
	subq	$64, %rsp
	movq	%rdi, %r12
	nop
.Lcesc_tail:
	movq	%rsp, %rbx
	movq	%rbx, (%r12)
	movq	$0, %rax
	addq	$64, %rsp
	popq	%r12
	popq	%rbx
	ret
	.size	cesc_owner, .-cesc_owner
	.section	.note.GNU-stack,"",@progbits
