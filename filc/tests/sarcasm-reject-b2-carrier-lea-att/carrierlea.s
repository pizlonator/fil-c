# Lea escape through a clone carrier (fail-closed): the clone parks the
# jump-site rsp in callee-saved %rbx (a legal carrier save) and then computes
# `leaq 0(%rbx), %rax` — address arithmetic on the phantom value, the lea
# spelling of reading the carrier. The lea-save early-return requires an %rsp
# (or %rbp-frame) base, so a carrier base can never be swallowed into a save;
# the address computation falls through to the use-check, which rejects it
# (aliasSlotOf refuses leas outright). Static error at the lea. Companion to
# sarcasm-reject-b2-carrier-escape-att (register store of the carrier
# itself): together they pin every non-discipline use of a live carrier.
	.text
	.globl	clea_jump
	.type	clea_jump, @function
clea_jump:                      ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	jmp	.Lclea_tail
	.size	clea_jump, .-clea_jump
	.globl	clea_owner
	.type	clea_owner, @function
clea_owner:                     ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	nop
.Lclea_tail:
	movq	%rsp, %rbx
	leaq	0(%rbx), %rax
	movq	%rbx, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	clea_owner, .-clea_owner
	.section	.note.GNU-stack,"",@progbits
