	.text

# ---------------------------------------------------------------------------
# The camellia CBC residue-tail shapes (crypto/camellia/asm/cmll-x86_64.pl's
# .Lcbc_enc_pushf / .Lcbc_dec_pushf) with the FULL pristine wrap: the helper
# enters with an arbitrary caller DF, pushfq saves ALL the flags (DF with
# them), the cld makes the checked rep movsb run FORWARD, and the popfq
# restores the caller's DF with the rest. The checked rep must not trap even
# when the caller violated the ABI and left DF=1 (the DF=1-with-count trap is
# pinned by sarcasm-cmll-df-att; here the cld is present, so no trap may
# fire).
#
# Model notes (FIP CC word placement): for void(ptr, ptr, long) the first
# integer-class word arrives in %rdx (arg1's intval) and %rcx holds arg1's
# capability LOWER, so the rep count is loaded into %rcx from %rdx (arg3) -
# redefining the body's %rcx web (which the FIP seeded with arg1's lower) is
# exactly what the hardware does, and `in` is not accessed afterwards.

# encrypt-shaped residue tail: %rsi <- in, %rdi <- ivec + 8.
	.globl	cam_enc_tail
	.type	cam_enc_tail, @function
cam_enc_tail:                   ;! void(ptr, ptr, long)
.Lcbc_enc_pushf:
	pushfq
	cld
	movq	%rdi, %r8         # park $inp
	leaq	8(%rsi), %rdi     # 8+$ivec -> %rdi
	movq	%r8, %rsi         # $inp -> %rsi
	movq	%rdx, %rcx        # the rep count (the length residue)
	rep movsb
	popfq
.Lcbc_enc_popf:
	ret
	.size	cam_enc_tail, .-cam_enc_tail

# decrypt-shaped residue tail: %rsi <- ivec + 8, %rdi <- out.
	.globl	cam_dec_tail
	.type	cam_dec_tail, @function
cam_dec_tail:                   ;! void(ptr, ptr, long)
.Lcbc_dec_pushf:
	pushfq
	cld
	movq	%rdx, %rcx
	leaq	8(%rsi), %rsi     # 8+$ivec -> %rsi
	leaq	(%rdi), %rdi      # $out -> %rdi
	rep movsb
	popfq
.Lcbc_dec_popf:
	ret
	.size	cam_dec_tail, .-cam_dec_tail

# ABI-violating callers: they set DF=1 (via std) and call the helpers. The
# helpers' cld must prevent any DF=1 trap, and the helpers' popfq must hand
# the caller's DF=1 back; the trailing cld is test hygiene (see the main).
	.globl	cam_enc_std
	.type	cam_enc_std, @function
cam_enc_std:                    ;! void(ptr, ptr, long)
	std
	call	cam_enc_tail       ;! void(ptr, ptr, long)
	cld
	ret
	.size	cam_enc_std, .-cam_enc_std

	.globl	cam_dec_std
	.type	cam_dec_std, @function
cam_dec_std:                    ;! void(ptr, ptr, long)
	std
	call	cam_dec_tail       ;! void(ptr, ptr, long)
	cld
	ret
	.size	cam_dec_std, .-cam_dec_std
	.section	.note.GNU-stack,"",@progbits
