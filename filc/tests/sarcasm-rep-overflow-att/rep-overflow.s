	.file	"rep-overflow.c"
	.text
	# Count-overflow soundness: a %rcx whose byte count %rcx*elem wraps mod
	# 2^64 (2^61 qwords wrap to 0 bytes, 2^62 dwords, 2^63 words) must trap
	# in the lowering's count-overflow guard. The wrapped count would pass
	# the room checks while the hardware copies %rcx elements OOB, so the
	# guard must fire first and the hardware copy must never run.
	.globl	rep_movsq_ov
	.type	rep_movsq_ov, @function
rep_movsq_ov:                   ;! void(ptr, ptr, size_t)
	movq	%rdx, %rcx
	rep movsq
	ret
	.size	rep_movsq_ov, .-rep_movsq_ov
	.globl	rep_movsl_ov
	.type	rep_movsl_ov, @function
rep_movsl_ov:                   ;! void(ptr, ptr, size_t)
	movq	%rdx, %rcx
	rep movsl
	ret
	.size	rep_movsl_ov, .-rep_movsl_ov
	.globl	rep_movsw_ov
	.type	rep_movsw_ov, @function
rep_movsw_ov:                   ;! void(ptr, ptr, size_t)
	movq	%rdx, %rcx
	rep movsw
	ret
	.size	rep_movsw_ov, .-rep_movsw_ov
	.globl	rep_stosq_ov
	.type	rep_stosq_ov, @function
rep_stosq_ov:                   ;! void(ptr, size_t, long)
	movq	%rsi, %rcx
	movq	%rdx, %rax
	rep stosq
	ret
	.size	rep_stosq_ov, .-rep_stosq_ov
	.section	.note.GNU-stack,"",@progbits
