# Escaping banded lea through a global (fail-closed): the clone computes an
# address inside its own inner frame (`leaq 8(%rsp), %rax` — banded
# coordinates) and publishes it in a global slot, where any later dereference
# (even after the frame is torn down and recycled) would read garbage. Taking
# the address is an escape and a static error at the lea itself.
# ADDRESS ARITHMETIC (why the unbanded deref would be wrong): the clone's
# `leaq 8(%rsp)` keys unbiased off 8 + D0 - d = 8, but the band layout gives
# this single-extent clone base 192, so the lea computes banded frame offset
# 200 — band scratch 128 past the 64-byte prologue frame. The same spelling
# outside the band (a later unbanded deref of the published global) resolves
# to offset 8 of the surrounding frame: off by the whole band base — and
# read after the frame is recycled. The stored address and any unbanded deref
# of it name different memory, so taking the address is a static error at the
# lea itself.
	.text
	.globl	eglb_jump
	.type	eglb_jump, @function
eglb_jump:                      ;! long()
	pushq	%rbx
	subq	$64, %rsp
	jmp	.Leglb_tail
	.size	eglb_jump, .-eglb_jump
	.globl	eglb_owner
	.type	eglb_owner, @function
eglb_owner:                      ;! long()
	pushq	%rbx
	subq	$64, %rsp
	nop
.Leglb_tail:
	leaq	8(%rsp), %rax
	movq	%rax, eglb_slot(%rip)
	movq	$0, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	eglb_owner, .-eglb_owner
	.data
	.globl	eglb_slot
eglb_slot:
	.quad	0
	.section	.note.GNU-stack,"",@progbits
