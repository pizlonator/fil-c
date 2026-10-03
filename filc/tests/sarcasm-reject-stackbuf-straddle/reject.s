	.text
# A static access straddling the buffer's boundary: rejected as ambiguous.
	.globl	f
	.type	f, @function
f:                              ;! int(size_t)
	subq	$64, %rsp
	movl	(%rsp,%rdi), %eax #! stack buffer (x, %rsp, %rsp + 32)
	movq	28(%rsp), %rbx
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
