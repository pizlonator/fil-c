# Chacha shape: dynamic 64-byte frame alignment COMBINED with incoming
# fast-CC stack-argument words. Nine longs are nine dense GPR words: the first
# four arrive in registers, the remaining five on the stack
# ([entry_rsp + 8 + 8*w]). The `and $-64, %rsp` note plus the vmovdqa64
# cluster force layout.alignA = 64, so the body rsp is the post-and rsp
# (pre-and rsp minus entry-dependent 0..63 slack). The five entry loads must
# read through the prologue-saved pre-and rsp (saveOff(%rsp)); a static
# rsp-relative displacement reads true-slack garbage on every entry residue
# except one (GDB: correct iff entry%64 == 56), corrupting the sum.
#
# Vacuous-proof two ways. First, the zmm stores/loads are REAL materialized
# vmovdqa64 accesses: a misaligned frame raises #GP on the hardware, so the
# test faults instead of silently passing. Second, the C caller sweeps the
# entry across 128 recursion depths (every mod-64 residue many times over):
# any static-displacement entry load is wrong on ~7/8 of the levels and the
# data check fails deterministically.
	.text
	.globl	sum9align64
	.type	sum9align64, @function
sum9align64:                    ;! long(long,long,long,long,long,long,long,long,long)
	movq	8(%rsp), %r10
	movq	16(%rsp), %r11
	movq	24(%rsp), %rax
	pushq	%rbx
	movq	%rax, %rbx
	subq	$256, %rsp
	and	$-64, %rsp
	vpxorq	%zmm0, %zmm0, %zmm0
	vmovdqa64	%zmm0, 0(%rsp)
	vmovdqa64	0(%rsp), %zmm1
	vmovdqa64	%zmm1, 64(%rsp)
	vmovdqa64	64(%rsp), %zmm2
	vmovq	%xmm2, %rax
	addq	%rdi, %rax
	addq	%rsi, %rax
	addq	%rdx, %rax
	addq	%rcx, %rax
	addq	%r8, %rax
	addq	%r9, %rax
	addq	%r10, %rax
	addq	%r11, %rax
	addq	%rbx, %rax
	addq	$256, %rsp
	popq	%rbx
	ret
	.size	sum9align64, .-sum9align64
	.section	.note.GNU-stack,"",@progbits
