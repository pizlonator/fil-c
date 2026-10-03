	.text
# Two annotations on one instruction: the second is not silently swallowed.
	.globl	f
	.type	f, @function
f:                              ;! int(size_t)
	subq	$64, %rsp
	movq	%rdi, 8(%rsp) #! stack buffer (x, %rsp, %rsp + 32) ;! store ptr
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
