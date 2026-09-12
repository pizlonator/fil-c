# Slot carrier across a real call (the accepted side of the slot-carrier
# discipline): the clone parks the jump-site rsp in %rbx, spills it to
# 8(%rsp), calls across the spill, reloads the carrier from the slot, and
# recovers %rsp through it. Frame memory survives calls, so slot carriers
# are never poisoned — the reload revives the exact entry depth and the
# clone returns normally. Executes through the join to prove the reloaded
# carrier recovers the right rsp (a clobbered slot would misaddress the
# teardown and crash). Companion to the rejected
# sarcasm-reject-b2-carrier-memslot-att (reading the slot AS the call
# target, which observes the phantom value).
	.text
	.globl	cslot_jump
	.type	cslot_jump, @function
cslot_jump:                      ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	jmp	.Lcslot_tail
	.size	cslot_jump, .-cslot_jump
	.globl	cslot_owner
	.type	cslot_owner, @function
cslot_owner:                     ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	nop
.Lcslot_tail:
	movq	%rsp, %rbx
	movq	%rbx, 8(%rsp)
	call	cslot_helper      ;! long(long)
	movq	8(%rsp), %rbx
	movq	%rbx, %rsp
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	cslot_owner, .-cslot_owner
	.globl	cslot_helper
	.type	cslot_helper, @function
cslot_helper:                   ;! long(long)
	movq	%rdi, %rax
	addq	$1, %rax
	ret
	.size	cslot_helper, .-cslot_helper
	.section	.note.GNU-stack,"",@progbits
