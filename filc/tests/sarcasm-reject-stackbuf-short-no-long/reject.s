	.text
# A short form with no long form anywhere in the file: rejected.
	.globl	f
	.type	f, @function
f:                              ;! int(size_t)
	subq	$64, %rsp
	movl	(%rsp,%rdi), %eax #! stack buffer (q)
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
