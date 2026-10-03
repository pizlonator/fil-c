# Aligned vector traffic in a MID-FUNCTION `and $-32, %rsp` frame. The
# unconditional jump ends the prologue prefix (same shape as
# sarcasm-frame-and-alias-after-att), so the and is a mid-function note: the
# frameAndSeen tracking (not the frameAndDepth value, which here holds the
# depth of a mid-function and) keys the vmovdqa cluster at the and base, the
# emitted .yolo.s keeps the dynamic `subq`+`andq $-32` and materializes the
# cluster at displacement 576 ≡ 0 mod 32, and the epilogue's static `addq` is
# rewritten into a restore of the parked pre-and rsp. Before the frameAndSeen
# fix the "no note" sentinel (frameAndDepth==0) was also conflated with
# mid-function notes whose depth is 0; this shape pins the mid-function flavor.
#
# Vacuous-proof like sarcasm-frame-and32-att: the vmovdqa pair is REAL
# materialized aligned traffic (a misaligned vmovdqa raises #GP, so a
# regression faults instead of silently passing), and the C caller sweeps 64
# recursion levels of verified canary wall plus the heap data round-trip on
# every level.
	.text
	.globl	midand_test
	.type	midand_test, @function
midand_test:                    ;! void(ptr,ptr)
	pushq	%rbx
	subq	$256, %rsp
	jmp	.Lafter
.Lafter:
	and	$-32, %rsp
	vmovupd	(%rdi), %ymm0
	vmovdqa	%ymm0, 0(%rsp)
	vmovdqa	0(%rsp), %ymm1
	vmovupd	%ymm1, (%rsi)
	xorl	%eax, %eax
	addq	$256, %rsp
	popq	%rbx
	ret
	.size	midand_test, .-midand_test
	.section	.note.GNU-stack,"",@progbits
