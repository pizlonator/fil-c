	.file	"andframe-fp.s"
	.text
# REJECT: an FP/SIMD access demanding more alignment than the synthesized
# frame can back, inside a stack buffer.
#
# fnH's only FP stack traffic is one 32-byte vmovdqa store into the buffer.
# Buffer groups are placed so every aligned access lands on an aligned byte
# of the synthesized frame, and a group's own requirement feeds the frame's
# effective alignment — but the effective alignment is capped at 16 when
# nothing else in the body needs more (no materialized cluster carries a
# wider requirement here, and there is no `and $-N, %rsp` note), because the
# entry rsp is only 16-predictable. A 32-byte requirement therefore cannot be
# placed and fails closed. (Bodies that genuinely back the width — an
# `and $-32/%-64` frame with the group's accesses on the aligned paths, like
# OpenSSL's ChaCha20_8x/16x — are accepted; see
# sarcasm-stackbuf-andframe-att for the GPR-only and-frame case.)
#
# sarcasm: stack buffer group [.., ..) needs 32-byte alignment but the
# synthesized frame only guarantees 16
	.globl	fnH
	.type	fnH, @function
fnH:                            ;! long(size_t, size_t)
	subq	$128, %rsp
	movl	$0x11111111, 0(%rsp)
	movl	$0x22222222, 4(%rsp)
	vmovdqa	%ymm0, 32(%rsp)
	movl	(%rsp,%rsi), %eax #! stack buffer (kh, %rsp, %rsp + 64)
	addq	$128, %rsp
	ret
	.size	fnH, .-fnH
	.section	.note.GNU-stack,"",@progbits
