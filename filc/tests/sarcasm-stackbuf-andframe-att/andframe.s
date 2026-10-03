	.file	"andframe.s"
	.text
# `#! stack buffer (...)` in an `and $-N, %rsp`-aligned frame, GPR traffic.
#
# fnG dynamically aligns its frame on ONE path (`andq $-32, %rsp` behind a
# conditional branch, the classic perlasm alignment idiom). The `and` shifts
# rsp by a dynamic slack the normalized coordinates do not model, which used
# to fail-close every buffer declaration in such a frame. The relaxation that
# makes this test work: the buffer's accesses here are all GPR (scalar
# integer loads/stores), which carry no alignment promise, so the buffer
# group is placed modulo 1 and the `and`'s dynamic slack is irrelevant to it.
# All of the buffer's post-`and` accesses shift together with the slack, so
# the bytes stay consistent on both paths: the model's normalized offsets are
# the same on the aligned and the unaligned path, and each path's
# stores/loads pair up through the same redirected region bytes.
#
# The aligned-path counterpart (an FP/SIMD access demanding more alignment
# than the synthesized frame backs) stays rejected — see
# sarcasm-reject-stackbuf-andframe-fp.
	.globl	fnG
	.type	fnG, @function
fnG:                            ;! long(size_t, size_t)
	movq	%rsp, %r10          # park the entry rsp for the teardown
	subq	$128, %rsp
	testq	%rdi, %rdi
	jz	.Lg_plain
	andq	$-32, %rsp          # dynamically align (one path only)
.Lg_plain:
	movl	$0x11111111, 0(%rsp)
	movl	$0x22222222, 4(%rsp)
	movl	$0x33333333, 8(%rsp)
	movl	$0x44444444, 12(%rsp)
	# GPR indexed load + static load through the buffer: no alignment
	# promise, so the and's slack cannot break it.
	movl	(%rsp,%rsi), %eax #! stack buffer (kg, %rsp, %rsp + 64)
	movl	4(%rsp), %ecx
	addl	%ecx, %eax
	movq	%r10, %rsp          # restore the entry rsp (both paths)
	ret
	.size	fnG, .-fnG
	.section	.note.GNU-stack,"",@progbits
