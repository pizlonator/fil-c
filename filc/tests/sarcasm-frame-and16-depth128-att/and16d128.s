# Regression test for the 16-byte `and $-N, %rsp` note at a depth of 0 mod 16
# (the SysV-keying discriminator): `mov %rsp,%r11; subq $128,%rsp;
# andq $-16,%rsp` puts the note at rsp depth 128 (128 ≡ 0 mod 16). The
# post-`and` rsp is 16-aligned ABSOLUTELY (the and clears the low bits), so an
# areq-16 access keys at the note's own base: off ≡ D0 - 128 (mod 16), which
# for D0 = 128 is off ≡ 0 (mod 16). The SysV base (D0 - 8) only coincides with
# that residue when the note sits at depth ≡ 8 (mod 16) — which the sibling
# and16d136 test's 136 is, so there the un-noted SysV keying happens to agree
# and emits byte-identical code — but at depth 128 ≡ 0 (mod 16) the note's own
# base (≡ 0 mod 16) and the SysV base (≡ 8 mod 16) differ by 8: keying the
# note at D0 - 8 misplaces every movaps by 8 mod 16, so the movaps emit
# ≡ 8 (mod 16) and #GP at runtime (an uncatchable fault). This test therefore
# passes when 16-byte notes key at the note's own depth and fails (SIGSEGV)
# if they regress to SysV keying.
#
# Vacuous-proof two ways. First, the movaps are REAL materialized aligned
# accesses: a misaligned movaps raises #GP (an uncatchable fault), so any
# regression faults the test instead of silently passing. Second, the C caller
# wraps every call in a swept stack-depth canary wall (see and16d128-main.c):
# 128 recursion levels each own a verified 256-byte wall segment directly above
# the call's entry rsp, so an escaping cluster corrupts verified bytes.
	.text
	.globl	and16d128_test
	.type	and16d128_test, @function
and16d128_test:                 ;! void(ptr,ptr)
	mov	%rsp, %r11
	subq	$128, %rsp
	andq	$-16, %rsp
	movq	%r11, 64(%rsp)
	vmovupd	(%rdi), %xmm0
	vmovaps	%xmm0, 0(%rsp)
	vmovaps	0(%rsp), %xmm1
	vmovaps	%xmm1, 16(%rsp)
	vmovaps	16(%rsp), %xmm2
	vmovaps	%xmm2, 48(%rsp)
	vmovaps	48(%rsp), %xmm3
	vmovaps	%xmm3, 112(%rsp)
	vmovaps	112(%rsp), %xmm4
	vmovupd	%xmm4, (%rsi)
	movq	64(%rsp), %r11
	mov	%r11, %rsp
	ret
	.size	and16d128_test, .-and16d128_test
	.section	.note.GNU-stack,"",@progbits
