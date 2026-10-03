	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	subq	$64, %rsp
	movq	%rdi, -8(%rsp)
	# `;! store ptr` on a non-move instruction: the frame slot virtualizes
	# into a register, so only a plain register<->slot move can carry the
	# capability. (An RMW pointer slot needs real memory -- `;! load store
	# ptr` on a heap/region address.)
	addq	%rax, -8(%rsp)     ;! store ptr
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
