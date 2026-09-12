# Scratch-liveness enforcement for the SARCASM-only ChaCha byte tails
# (projects/openssl-3.6.4/crypto/chacha/asm/chacha-x86_64.pl,
# chacha_tail_sarcasm): %rcx enters holding a LIVE 64-bit value — the
# function-pointer argument, whose upper 32 may be nonzero — exactly the
# shape a tail entry with a live scratch would have. The tail-shaped
# clobber (`movl 32(%rsp), %ecx`, zeroing the upper 32 in hardware,
# followed by the tail's `shrq $8, %rcx` byte-loop shift) is a FULL
# 64-bit kill in sarcasm's model too (x86_64_isa.partialRegWrite is
# width<32 only, so `movl` purely defines its web), hence the later
# 64-bit use of %rcx must fail: the indirect call below is rejected at
# compile time ("must be a function pointer value") because the killed
# web no longer carries one. Without the clobber the same call compiles
# (the pointer web flows through), so a green compileFailure run pins
# that the kill — not the pointer kind — is what fails. This is the
# executable half of the per-tail dead-scratch proof whose other half is
# the generation-time die in chacha_tail_sarcasm (scratch aliasing a
# live counter) plus the real-vector runs in
# filc/tests/sarcasm-chacha-vec-att. (A `;! load ptr` through the
# clobbered register instead lowers to a static access-check failure —
# fail-closed at runtime with "filc safety error" — rather than a
# compile rejection; both directions fail, this test pins the
# compile-time one.)
	.text
	.globl	chacha_scratch_live
	.type	chacha_scratch_live, @function
chacha_scratch_live:            ;! long(ptr, long)
	subq	$64, %rsp
	movq	%rdi, %rcx
	movl	32(%rsp), %ecx
	shrq	$8, %rcx
	call	*%rcx ;! long(long)
	addq	$64, %rsp
	ret
	.size	chacha_scratch_live, .-chacha_scratch_live
	.section	.note.GNU-stack,"",@progbits
