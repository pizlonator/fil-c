	.text
# A `store ptr` on an access inside a buffer range: buffer slots cannot hold
# capabilities.
	.globl	f
	.type	f, @function
f:                              ;! int(size_t)
	subq	$64, %rsp
	movq	%rdi, 8(%rsp) #! stack buffer (x, %rsp, %rsp + 32)
	movq	%rsi, 12(%rsp) #! store ptr
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
