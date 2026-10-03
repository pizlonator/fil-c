	.text
# Regression test for the keyed-aware alignment gate: an rbp-RELATIVE aligned
# access in an `and $-32, %rsp`-noted frame whose offset is out of phase in the
# SysV coordinates the rbp-relative cluster is placed in. The old code checked
# the and-base identity for such accesses and silently accepted this shape,
# emitting a misaligned vmovdqa (a #GP on every call). rbp holds the pre-and
# rsp, so -16(%rbp) executes at entry_rsp - 8 - 80 - 16: 16 mod 32 out of phase
# in the SysV base the access keys at, and the fix now rejects it exactly like
# an out-of-phase rsp-relative access. (The in-phase sibling -32(%rbp) is
# accepted and runs aligned — see sarcasm-frame-and32-vmovapd-att.)
#
# Rejects with: "instruction requires 32 byte alignment, but offset is out of
# phase by 16 bytes (use an aligned offset, or use the unaligned form (e.g.
# vmovdqu/vmovups))" on the `vmovdqa %ymm0, -16(%rbp)` store.
	.globl	rbpphase
	.type	rbpphase, @function
rbpphase:                       ;! long(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$80, %rsp
	andq	$-32, %rsp
	vmovdqa	%ymm0, -16(%rbp)
	movq	%rbp, %rsp
	popq	%rbp
	ret
	.size	rbpphase, .-rbpphase
	.section	.note.GNU-stack,"",@progbits
