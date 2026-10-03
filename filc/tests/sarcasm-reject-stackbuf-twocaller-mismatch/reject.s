	.text
# The same buffer id declared by TWO functions at DIFFERENT ranges: the
# file-wide canonical rule rejects the mismatch (the offsets must match).
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	subq	$32, %rsp
	movl	(%rsp,%rdi,4), %eax #! stack buffer (b, %rsp, %rsp + 16)
	addq	$32, %rsp
	ret
	.size	f, .-f
	.globl	g
	.type	g, @function
g:                              ;! long(long)
	subq	$48, %rsp
	movl	(%rsp,%rdi,4), %eax #! stack buffer (b, %rsp, %rsp + 24)
	addq	$48, %rsp
	ret
	.size	g, .-g
	.section	.note.GNU-stack,"",@progbits
