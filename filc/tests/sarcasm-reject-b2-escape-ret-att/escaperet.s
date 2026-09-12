# Escaping banded lea through the return register (fail-closed): the clone
# computes an address inside its own inner frame (`leaq 8(%rsp), %rax` —
# banded coordinates) and returns it. The caller would receive a pointer
# into scratch that the callee's frame teardown recycles, so taking the
# address is an escape and a static error at the lea itself.
# ADDRESS ARITHMETIC (why the unbanded deref would be wrong): the clone's
# `leaq 8(%rsp)` keys unbiased off 8 + D0 - d = 8, but the band layout gives
# this single-extent clone base 192, so the lea computes banded frame offset
# 200 — band scratch 128 past the 64-byte prologue frame. The same spelling
# outside the band (the helper's/caller's unbanded deref) resolves to offset 8
# of the surrounding frame: off by the whole band base. The stored address and
# any unbanded deref of it name different memory, so taking the address is a
# static error at the lea itself.
	.text
	.globl	eret_jump
	.type	eret_jump, @function
eret_jump:                      ;! ptr()
	pushq	%rbx
	subq	$64, %rsp
	jmp	.Leret_tail
	.size	eret_jump, .-eret_jump
	.globl	eret_owner
	.type	eret_owner, @function
eret_owner:                      ;! ptr()
	pushq	%rbx
	subq	$64, %rsp
	nop
.Leret_tail:
	leaq	8(%rsp), %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	eret_owner, .-eret_owner
	.section	.note.GNU-stack,"",@progbits
