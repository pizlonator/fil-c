# VEX-alias aligned spellings (`vmovapd`/`vmovaps`, the APD/APS forms) in an
# `and $-32, %rsp`-noted frame, plus an rbp-RELATIVE aligned access in such a
# frame. `vmovapd`/`vmovaps` are aliases of `vmovdqa`/`vmovdqa32` for the
# alignment-requiring scan (areq 32 and 16 respectively), so the first function
# proves both spellings materialize aligned (emitted displacements 576 and 608,
# both ≡ 0 mod 32) and keep their input alignment in the synthesized frame. The
# second function proves the rbp-relative flavor: `vmovdqa -32(%rbp)` in a
# push-rbp + sub + `andq $-32, %rsp` frame used to be false-rejected (the scan
# keyed every aligned access at the and base without considering that an
# rbp-relative access executes in SysV coordinates — rbp holds the pre-and rsp);
# it is now keyed at the SysV phase (−32(%rbp) is phase-0 there) and materialized
# into the and frame at an aligned output address.
#
# Vacuous-proof like sarcasm-frame-and32-att: the aligned accesses are REAL
# materialized accesses (a misaligned one raises #GP, so a regression faults
# instead of silently passing), two distinct live patterns ride two distinct
# slots, and the C caller sweeps 64 recursion levels of verified canary wall
# plus the data round-trip on every level.
	.text
	.globl	andapd_test
	.type	andapd_test, @function
andapd_test:                    ;! void(ptr,ptr)
	pushq	%rbx
	subq	$256, %rsp
	and	$-32, %rsp
	vmovupd	(%rdi), %ymm0
	vmovapd	%ymm0, 0(%rsp)
	vmovapd	0(%rsp), %ymm1
	vmovupd	%ymm1, (%rsi)
	vmovupd	(%rdi), %xmm0
	vmovaps	%xmm0, 64(%rsp)
	vmovaps	64(%rsp), %xmm2
	vmovupd	%xmm2, 32(%rsi)
	xorl	%eax, %eax
	addq	$256, %rsp
	popq	%rbx
	ret
	.size	andapd_test, .-andapd_test
	.globl	andapd_rbp_test
	.type	andapd_rbp_test, @function
andapd_rbp_test:                ;! void(ptr,ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$96, %rsp
	andq	$-32, %rsp
	vmovupd	(%rdi), %ymm0
	vmovdqa	%ymm0, -32(%rbp)
	vmovdqa	-32(%rbp), %ymm1
	vmovupd	%ymm1, (%rsi)
	movq	%rbp, %rsp
	popq	%rbp
	ret
	.size	andapd_rbp_test, .-andapd_rbp_test
	.section	.note.GNU-stack,"",@progbits
