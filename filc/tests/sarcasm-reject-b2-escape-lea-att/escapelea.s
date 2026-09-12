# Escaping banded lea (fail-closed): the clone computes an address inside
# its own inner frame (`leaq 8(%rsp), %rax` — banded coordinates, a different
# address from the same spelling in the jumper) and stores it to caller
# memory. The address has no meaning outside the clone's band, so taking it
# is an escape and a static error — the lea itself is rejected before the
# store is even reached.
# ADDRESS ARITHMETIC (why the unbanded deref would be wrong): the clone's
# `leaq 8(%rsp)` keys unbiased off 8 + D0 - d = 8, but the band layout gives
# this single-extent clone base 192, so the lea computes banded frame offset
# 200 — band scratch 128 past the 64-byte prologue frame. The same spelling
# outside the band (the helper's/caller's unbanded deref) resolves to offset 8
# of the surrounding frame: off by the whole band base. The stored address and
# any unbanded deref of it name different memory, so taking the address is a
# static error at the lea itself.
	.text
	.globl	esc_jump
	.type	esc_jump, @function
esc_jump:                       ;! long(ptr,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	jmp	.Lesc_tail
	.size	esc_jump, .-esc_jump
	.globl	esc_owner
	.type	esc_owner, @function
esc_owner:                      ;! long(ptr,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	nop
.Lesc_tail:
	leaq	8(%rsp), %rax
	movq	%rax, (%rbx)
	movq	$0, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	esc_owner, .-esc_owner
	.section	.note.GNU-stack,"",@progbits
