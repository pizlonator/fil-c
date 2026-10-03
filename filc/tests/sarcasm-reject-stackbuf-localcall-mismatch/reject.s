	.text
# The same buffer id declared INSIDE a local subroutine (the clone keys it in
# the subroutine's own frame coordinates: [0, 16)) and in the calling
# function's own body at a DIFFERENT range ([0, 24)): the file-wide canonical
# rule rejects the mismatch (the offsets must match) — the same name cannot
# name two different byte ranges.
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	pushq	%rbx
	subq	$32, %rsp
	movq	$0, 0(%rsp) #! stack buffer (b, %rsp, %rsp + 24)
	movl	%edi, %r10d
	call	m1
	movl	%r9d, %eax
	addq	$32, %rsp
	popq	%rbx
	ret
	.size	f, .-f
	.type	m1, @function
m1:
	subq	$48, %rsp
	movl	$7, (%rsp) #! stack buffer (b, %rsp, %rsp + 16)
	movl	(%rsp,%r10,4), %r9d #! stack buffer (b)
	addq	$48, %rsp
	ret
	.size	m1, .-m1
	.section	.note.GNU-stack,"",@progbits
