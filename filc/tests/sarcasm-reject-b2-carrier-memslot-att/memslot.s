# Memory-indirect call through a frame-slot (%rsp) carrier (fail-closed):
# the clone parks the jump-site rsp in %rbx and spills it to 8(%rsp) — a
# live slot carrier — then `call *8(%rsp)` reads the slot as a CODE address.
# The slot operand sits in call position 1, but it is a READ of the phantom
# value, not a write: misclassifying it as a store would silently drop
# (poison) the slot instead of rejecting the read. The slot-traffic check
# treats call/jmp memory operands as reads, so this is a static error at the
# call. Companion to sarcasm-reject-b2-carrier-memcall-att (register-carrier
# base): together they pin `call *mem` through every carrier spelling.
	.text
	.globl	mslot_jump
	.type	mslot_jump, @function
mslot_jump:                      ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	jmp	.Lmslot_tail
	.size	mslot_jump, .-mslot_jump
	.globl	mslot_owner
	.type	mslot_owner, @function
mslot_owner:                     ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	nop
.Lmslot_tail:
	movq	%rsp, %rbx
	movq	%rbx, 8(%rsp)
	call	*8(%rsp)          ;! long(long)
	movq	%rbx, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	mslot_owner, .-mslot_owner
	.section	.note.GNU-stack,"",@progbits
