	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	subq	$64, %rsp
	movq	%rdi, -8(%rsp)
	# The atomic ptr family on a frame slot: a virtualized slot is a register,
	# not real memory, so the runtime's atomic invisicap load/store has
	# nothing to point at.
	movq	-8(%rsp), %rax     ;! atomic load ptr
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
