	.file	"rep-movs.c"
	.text
	# rep movs* in every element width, plus the bare single-step form and
	# the %rcx == 0 no-op (which must not trap even on null pointers, and
	# even with the direction flag set).
	.globl	rep_movsq
	.type	rep_movsq, @function
rep_movsq:                      ;! void(ptr, ptr, size_t)
	movq	%rdx, %rcx
	rep movsq
	ret
	.size	rep_movsq, .-rep_movsq
	.globl	rep_movsb
	.type	rep_movsb, @function
rep_movsb:                      ;! void(ptr, ptr, size_t)
	movq	%rdx, %rcx
	rep movsb
	ret
	.size	rep_movsb, .-rep_movsb
	.globl	rep_movsw
	.type	rep_movsw, @function
rep_movsw:                      ;! void(ptr, ptr, size_t)
	movq	%rdx, %rcx
	rep movsw
	ret
	.size	rep_movsw, .-rep_movsw
	.globl	rep_movsl
	.type	rep_movsl, @function
rep_movsl:                      ;! void(ptr, ptr, size_t)
	movq	%rdx, %rcx
	rep movsl
	ret
	.size	rep_movsl, .-rep_movsl
	.globl	bare_movsq
	.type	bare_movsq, @function
bare_movsq:                     ;! void(ptr, ptr)
	movsq
	ret
	.size	bare_movsq, .-bare_movsq
	.globl	rep_zero_skip
	.type	rep_zero_skip, @function
rep_zero_skip:                  ;! void(ptr, ptr)
	# %rcx == 0 under DF=1: the instruction touches nothing, so neither the
	# bounds checks nor the direction-flag check may trap. DF is restored
	# before returning (the ABI requires DF=0 at call boundaries).
	std
	xorq	%rcx, %rcx
	rep movsq
	cld
	ret
	.size	rep_zero_skip, .-rep_zero_skip
	.section	.note.GNU-stack,"",@progbits
