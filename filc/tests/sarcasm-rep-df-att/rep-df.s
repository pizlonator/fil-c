	.file	"rep-df.c"
	.text
	# A set direction flag cannot be modeled by the checked rep lowering
	# (which copies/fills forward), so it traps with an illegal instruction
	# — unless the count is zero, in which case the instruction is a no-op
	# and must not trap.
	.globl	df_set_then_copy
	.type	df_set_then_copy, @function
df_set_then_copy:               ;! void(ptr, ptr, size_t)
	std
	movq	%rdx, %rcx
	rep movsq
	cld
	ret
	.size	df_set_then_copy, .-df_set_then_copy
	.section	.note.GNU-stack,"",@progbits
