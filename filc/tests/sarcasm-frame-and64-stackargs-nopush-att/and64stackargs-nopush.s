# Zero-push twin of the sarcasm-frame-and64-stackargs-att chacha shape: dynamic
# 64-byte frame alignment combined with an incoming fast-CC stack-argument
# word, but a minimal five-word body that folds every argument into one web
# BEFORE the aligned block and uses only caller-saved GPRs, so the allocator
# emits no callee-saved pushes and the entry load exercises the
# postPushDelta = 8*0 + 8 + 8*w path (the -att twin covers 8*1 + 8 + 8*w with
# its push %rbx). Five longs are five dense GPR words: the first four arrive in
# registers, the fifth on the stack ([entry_rsp + 8]). The `and $-64, %rsp`
# note plus the vmovdqa64 cluster force layout.alignA = 64, so the body rsp is
# the post-and rsp (pre-and rsp minus entry-dependent 0..63 slack). The entry
# load must read through the prologue-saved pre-and rsp (saveOff(%rsp)); a
# static rsp-relative displacement reads true-slack garbage on every entry
# residue except one, corrupting the sum.
#
# Vacuous-proof two ways. First, the zmm stores/loads are REAL materialized
# vmovdqa64 accesses: a misaligned frame raises #GP on the hardware, so the
# test faults instead of silently passing. Second, the C caller sweeps the
# entry across 128 recursion depths (every mod-64 residue many times over):
# any static-displacement entry load is wrong on ~7/8 of the levels and the
# data check fails deterministically.
	.text
	.globl	sum5align64_nopush
	.type	sum5align64_nopush, @function
sum5align64_nopush:             ;! long(long,long,long,long,long)
	addq	%rsi, %rdi
	addq	%rdx, %rdi
	addq	%rcx, %rdi
	addq	%r8, %rdi
	subq	$256, %rsp
	and	$-64, %rsp
	vpxorq	%zmm0, %zmm0, %zmm0
	vmovdqa64	%zmm0, 0(%rsp)
	vmovdqa64	0(%rsp), %zmm1
	vmovq	%xmm1, %rax
	addq	%rdi, %rax
	addq	$256, %rsp
	ret
	.size	sum5align64_nopush, .-sum5align64_nopush
	.section	.note.GNU-stack,"",@progbits
