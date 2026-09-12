# Banded-vs-unbanded ADDRESS COMPARE (the executable half of the escape
# proof): the jumper stores b in its own 0(%rsp) slot and parks a carrier;
# the clone stores a in its same-spelled 0(%rsp) slot (its private band),
# then reads BOTH — the banded slot directly and the jumper's slot through
# the absolute carrier — and returns their sum.
#
# The two reads spell the same displacement but must name DIFFERENT
# addresses: bandedOff = disp + D0 - d + bandBase (here 0 + 0 + 192) versus
# the carrier's absolute off 0. If banding collapsed (one coordinate for
# both), the clone's store would overwrite b and both reads would return a.
# The owner entry is the control case: there the tail addresses the owner's
# own frame, so both spellings coincide and the result is 2a — proving the
# test WOULD catch a collapse (the jumper path would also read 2a).
#
# This is why escaping a banded address (leaq into the clone's inner frame,
# stored to an argument/return/global — see sarcasm-reject-b2-escape-*)
# and dereferencing it unbanded is wrong: the stored address names band
# scratch, while any unbanded deref resolves into the surrounding frame
# (off by the whole band base), reading garbage. The rejections pin the
# escape; this test pins the address arithmetic behind them.
	.text
	.globl	acmp_jump
	.type	acmp_jump, @function
acmp_jump:                      ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	movq	%rsp, %r10
	jmp	.Lacmp_tail
	.size	acmp_jump, .-acmp_jump
	.globl	acmp_owner
	.type	acmp_owner, @function
acmp_owner:                     ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	movq	%rsp, %r10
	nop
.Lacmp_tail:
	movq	%rbx, 0(%rsp)
	movq	0(%rsp), %rax
	movq	0(%r10), %rcx
	addq	%rcx, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	acmp_owner, .-acmp_owner
	.section	.note.GNU-stack,"",@progbits
