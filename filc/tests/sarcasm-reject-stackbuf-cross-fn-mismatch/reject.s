	.text
# Two long forms of one id with different offsets (across functions): the
# offsets must match.
	.globl	f
	.type	f, @function
f:                              ;! int(size_t)
	subq	$64, %rsp
	movl	(%rsp,%rdi), %eax #! stack buffer (x, %rsp, %rsp + 32)
	addq	$64, %rsp
	ret
	.size	f, .-f
	.globl	g
	.type	g, @function
g:                              ;! int(size_t)
	subq	$64, %rsp
	movl	(%rsp,%rdi), %eax #! stack buffer (x, %rsp, %rsp + 48)
	addq	$64, %rsp
	ret
	.size	g, .-g
	.section	.note.GNU-stack,"",@progbits
