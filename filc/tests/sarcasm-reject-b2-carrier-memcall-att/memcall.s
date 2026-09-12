# Memory-indirect call through a clone carrier (fail-closed): `call
# *8(%rbx)` reads %rbx — which holds the dropped jump-site rsp save — as a
# CODE address. The carrier discipline's stack+offset alias use covers DATA
# accesses only: a control-flow read is never an alias, even when the
# displacement lands inside the frame (here off 8, which would otherwise
# legalize). The use-check rejects it at the call itself.
#
# ORDERING PROOF (reject-vs-poison): %rbx is callee-saved, so the call's
# caller-saved poisoning can never fire on it — the rejection here comes
# solely from the use-check iterating the incoming carrier map (reject
# before poison). A caller-saved carrier through the same shape would be
# poisoned afterwards, but the error still names the call, never a later
# use. Companion to sarcasm-reject-b2-carrier-indcall-att (register target),
# which pins the same ordering for `call *%reg`.
	.text
	.globl	mcall_jump
	.type	mcall_jump, @function
mcall_jump:                      ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	jmp	.Lmcall_tail
	.size	mcall_jump, .-mcall_jump
	.globl	mcall_owner
	.type	mcall_owner, @function
mcall_owner:                     ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	nop
.Lmcall_tail:
	movq	%rsp, %rbx
	call	*8(%rbx)          ;! long(long)
	movq	%rbx, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	mcall_owner, .-mcall_owner
	.section	.note.GNU-stack,"",@progbits
