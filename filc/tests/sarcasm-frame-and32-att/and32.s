# Prologue `and $-32, %rsp` alignment with aligned stack traffic: the and is
# dropped, the frame is emitted 32-aligned (the .yolo.s prologue shows the
# dynamic sequence `subq $N, %rsp; andq $-32, %rsp` with the pre-alignment rsp
# saved for the epilogue restore), and the vmovdqa slots keep their input
# alignment (plus an rsp+offset alias dance through %rax).
#
# This test is vacuous-proof three ways. First, every stack slot here is a REAL
# materialized vmovdqa access (FP-taint materialization, not a virtualized GPR
# slot): if the output frame were not 32-aligned, the hardware raises #GP on
# the misaligned vmovdqa — an uncatchable fault, so the test fails instead of
# silently passing. Second, four distinct slots (two direct, two through the
# alias) hold two distinct live patterns simultaneously: any shift/layout pad
# mismatch that overlaps slots, spills, or the GC root area corrupts a pattern
# or a root lower (the heap stores through the rooted %rdi then trap on the
# failed access check), so the C-side data check fails deterministically.
# Removing the `and` note does not silently pass either: the taint scan
# rejects the 32-byte aligned accesses outright (compile failure). Third, the
# C caller wraps every call in a swept stack-depth plus a canary wall (see
# and32-main.c): each of 128 C-recursion levels owns a 256-byte wall segment
# in its own frame, directly above the call's entry rsp — `volatile` with its
# address escaped through an asm barrier, so it stays a real live
# across-the-call stack object — so any
# aligned-cluster escape past the frame end crosses verified bytes and is
# caught. (Reading the aligned rsp itself back into C is
# not an option: the frame pass rejects any rsp-as-value read — `movq
# %rsp, %rax`, `movl %esp, %eax`, `pushq %rsp` — as a frame-address escape,
# so the bounds proof here is the canary wall, and the width proof is the
# #GP on the real aligned accesses.)
	.text
	.globl	and32_test
	.type	and32_test, @function
and32_test:                     ;! long(ptr)
	pushq	%rbx
	subq	$256, %rsp
	and	$-32, %rsp
	leaq	-128(%rsp), %rax
	vmovdqu	(%rdi), %ymm0
	vmovdqa	%ymm0, 0(%rsp)
	vpxor	%ymm1, %ymm1, %ymm1
	vmovdqa	%ymm1, 32(%rsp)
	vmovdqa	0(%rsp), %ymm2
	vmovdqa	%ymm2, 192(%rax)
	vmovdqa	32(%rsp), %ymm3
	vmovdqa	%ymm3, 224(%rax)
	vmovdqa	192(%rax), %ymm4
	vmovdqu	%ymm4, (%rdi)
	vmovdqa	224(%rax), %ymm5
	vmovdqu	%ymm5, 32(%rdi)
	xorl	%eax, %eax
	addq	$256, %rsp
	popq	%rbx
	ret
	.size	and32_test, .-and32_test
	.section	.note.GNU-stack,"",@progbits
