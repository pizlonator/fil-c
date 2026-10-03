	.text
# The incoming-stack-argument flavor of the aligned-access rejection: a
# 32-byte aligned vector read of the incoming stack-argument words. Nine longs
# are nine dense GPR words: the first six arrive in registers, the remaining
# three on the stack ([entry_rsp + 8 + 8*w]) — and the incoming stack is only
# ever 16-byte aligned (the caller's rsp at the call is 8 mod 16 plus the
# return address), so no `and $-N, %rsp` note inside THIS function can prove
# anything about the bytes below entry_rsp: the aligned form is rejected
# outright (the unaligned vmovdqu32 form would be fine).
#
# Rejects with: "stack alignment is only 16 bytes but instruction requires 32
# byte alignment (incoming stack arguments are only 16-byte aligned; use the
# unaligned form)" on the `vmovdqa32 8(%rsp), %ymm0` entry read. (Probed: the
# store flavor of the same shape — `vmovdqa32 %ymm0, 8(%rsp)` — is rejected
# too, by the ordinary frame-alignment gate: "stack alignment is only 16 bytes
# but instruction requires 32 byte alignment (align the frame with
# `and $-32, %rsp` in the prologue, or use the unaligned form
# (e.g. vmovdqu/vmovups))".)
	.globl	sum9
	.type	sum9, @function
sum9:                           ;! long(long,long,long,long,long,long,long,long,long)
	vmovdqa32	8(%rsp), %ymm0
	movq	%rdi, %rax
	addq	%rsi, %rax
	addq	%rdx, %rax
	addq	%rcx, %rax
	addq	%r8, %rax
	addq	%r9, %rax
	movq	8(%rsp), %r10
	addq	%r10, %rax
	movq	16(%rsp), %r10
	addq	%r10, %rax
	movq	24(%rsp), %r10
	addq	%r10, %rax
	vzeroupper
	ret
	.size	sum9, .-sum9
	.section	.note.GNU-stack,"",@progbits
