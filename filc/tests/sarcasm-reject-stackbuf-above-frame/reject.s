	.text
# A buffer extending above the function's own frame (past the prologue-end
# depth): rejected.
	.globl	f
	.type	f, @function
f:                              ;! int(size_t)
	subq	$64, %rsp
	movl	(%rsp,%rdi), %eax #! stack buffer (x, %rsp, %rsp + 128)
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
