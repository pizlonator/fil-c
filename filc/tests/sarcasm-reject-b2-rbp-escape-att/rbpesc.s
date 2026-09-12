# Frame-pointer escape in a B2 clone (fail-closed): the clone reads %rbp
# itself as a value while the jumper's frame pointer is established. The
# parked frame address has no output meaning (sarcasm synthesizes its own
# frame), so taking it is an escape and a static error — like any other
# frame-address-taking lea.
	.text
	.globl	rbpe_jump
	.type	rbpe_jump, @function
rbpe_jump:                      ;! long(long)
	pushq	%rbp
	movq	%rsp, %rbp
	movq	%rdi, %rcx
	jmp	.Lrbpe_tail
	.size	rbpe_jump, .-rbpe_jump
	.globl	rbpe_owner
	.type	rbpe_owner, @function
rbpe_owner:                     ;! long(long)
	pushq	%rbp
	movq	%rsp, %rbp
	movq	%rdi, %rcx
	nop
.Lrbpe_tail:
	movq	%rbp, %rax
	addq	%rcx, %rax
	leave
	ret
	.size	rbpe_owner, .-rbpe_owner
	.section	.note.GNU-stack,"",@progbits
