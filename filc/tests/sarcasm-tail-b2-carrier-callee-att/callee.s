# Callee-saved clone carrier across an annotated call (the accepted side of
# the carrier-call discipline): the clone parks the jump-site rsp in %rbx —
# legal only by the B2-clone relaxation — and the call preserves %rbx (every
# callee sarcasm can call does: same-file functions are compiled by sarcasm,
# externs by the SysV ABI), so the recovery revives the exact entry depth
# and the clone returns through the shared teardown. Executes through the
# join to prove the preserved carrier recovers the right rsp (a clobbered
# carrier would misaddress the teardown and crash).
	.text
	.globl	cse_jump
	.type	cse_jump, @function
cse_jump:                       ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	jmp	.Lcse_tail
	.size	cse_jump, .-cse_jump
	.globl	cse_owner
	.type	cse_owner, @function
cse_owner:                      ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	nop
.Lcse_tail:
	movq	%rsp, %rbx
	call	cse_helper        ;! long(long)
	movq	%rbx, %rsp
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	cse_owner, .-cse_owner
	.globl	cse_helper
	.type	cse_helper, @function
cse_helper:                     ;! long(long)
	movq	%rdi, %rax
	addq	$1, %rax
	ret
	.size	cse_helper, .-cse_helper
	.section	.note.GNU-stack,"",@progbits
