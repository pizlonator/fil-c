# Regression test for the 16-byte `and $-N, %rsp` note at a depth of 8 mod 16
# (the OpenSSL aesni prologue shape): `mov %rsp,%r11; pushq %rbp;
# subq $128,%rsp; andq $-16,%rsp` puts the note at rsp depth 136 (8 + 128,
# 136 ≡ 8 mod 16). The post-`and` rsp is 16-aligned ABSOLUTELY (the and clears
# the low bits), so an areq-16 access keys at the note's own base: off ≡
# D0 - 136 (mod 16), which for D0 = 136 is off ≡ 0 (mod 16). The SysV base
# (D0 - 8) only coincides with that residue when the note sits at depth ≡ 8
# (mod 16) — which 136 is, so the un-noted SysV keying happens to agree here —
# but the WIP keyed every access of a 16-byte note at frameAndDepth == 0
# (frameAndDepth was only recorded for N > 16 notes), i.e. at the base
# D0 - 0 = 136 ≡ 8 (mod 16) off from the truth: every movaps/movdqa in the
# aesni kernels emitted displacements misaligned by 8 and #GP'd at runtime
# (the openssl sm4-ofb SIGSEGV). The scan now records a 16-byte note's depth
# too, so the cluster keys and places at the real base and the movaps offsets
# emit ≡ 0 (mod 16).
#
# Vacuous-proof two ways. First, the movaps are REAL materialized aligned
# accesses: a misaligned movaps raises #GP (an uncatchable fault), so any
# regression faults the test instead of silently passing. Second, the C caller
# wraps every call in a swept stack-depth canary wall (see and16d136-main.c):
# 128 recursion levels each own a verified 256-byte wall segment directly above
# the call's entry rsp, so an escaping cluster corrupts verified bytes.
	.text
	.globl	and16d136_test
	.type	and16d136_test, @function
and16d136_test:                 ;! void(ptr,ptr)
	mov	%rsp, %r11
	pushq	%rbp
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
	.size	and16d136_test, .-and16d136_test
	.section	.note.GNU-stack,"",@progbits
