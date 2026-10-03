	.text
# A buffer reaching below the frame base (into the red zone): rejected.
	.globl	f
	.type	f, @function
f:                              ;! int(size_t)
	subq	$64, %rsp
	movl	(%rsp,%rdi), %eax #! stack buffer (x, %rsp - 16, %rsp + 16)
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
