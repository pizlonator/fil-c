# Escaping banded lea through a call argument (fail-closed): the clone
# computes an address inside its own inner frame (`leaq 8(%rsp), %rax` —
# banded coordinates, a different address from the same spelling in the
# jumper) and passes it to a helper that dereferences it. The banded address
# has no meaning outside the clone's band — the helper would read the
# jumper's frame slot or unbacked scratch — so taking it is an escape and a
# static error at the lea itself, no matter which sink (store, argument,
# return, global) the address flows to.
# ADDRESS ARITHMETIC (why the unbanded deref would be wrong): the clone's
# `leaq 8(%rsp)` keys unbiased off 8 + D0 - d = 8, but the band layout gives
# this single-extent clone base 192, so the lea computes banded frame offset
# 200 — band scratch 128 past the 64-byte prologue frame. The same spelling
# outside the band (the helper's/caller's unbanded deref) resolves to offset 8
# of the surrounding frame: off by the whole band base. The stored address and
# any unbanded deref of it name different memory, so taking the address is a
# static error at the lea itself.
	.text
	.globl	earg_jump
	.type	earg_jump, @function
earg_jump:                      ;! long(ptr,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	jmp	.Learg_tail
	.size	earg_jump, .-earg_jump
	.globl	earg_owner
	.type	earg_owner, @function
earg_owner:                     ;! long(ptr,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	nop
.Learg_tail:
	leaq	8(%rsp), %rax
	movq	%rax, %rdi
	call	earg_helper       ;! long(ptr)
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	earg_owner, .-earg_owner
	.globl	earg_helper
	.type	earg_helper, @function
earg_helper:                    ;! long(ptr)
	movq	(%rdi), %rax
	ret
	.size	earg_helper, .-earg_helper
	.section	.note.GNU-stack,"",@progbits
