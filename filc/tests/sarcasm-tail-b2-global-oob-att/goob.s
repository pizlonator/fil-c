# B2 clone reading past the end of a global object (fail at runtime): the
# object is 12 bytes, so an 8-byte load at +64 is out of bounds. Global
# accesses are capability-checked against the object's extent — inside or
# outside a clone alike — so the clone's OOB read traps exactly like the
# jumper's would. Companion to sarcasm-tail-b2-global-att, which pins the
# in-bounds (misaligned) side.
	.text
	.globl	goob_jump
	.type	goob_jump, @function
goob_jump:                      ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	jmp	.Lgoob_tail
	.size	goob_jump, .-goob_jump
	.globl	goob_owner
	.type	goob_owner, @function
goob_owner:                     ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	nop
.Lgoob_tail:
	movq	GOOBCAP+64(%rip), %rax
	addq	%rbx, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	goob_owner, .-goob_owner
	.data
	.p2align	2
GOOBCAP:
	.long	0x11111111
	.long	0x22222222
	.long	0x33333333
	.section	.note.GNU-stack,"",@progbits
