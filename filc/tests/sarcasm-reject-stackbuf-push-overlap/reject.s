	.text
# A buffer overlapping the outstanding push/callee-save slot area at the top
# of the frame: rejected.
	.globl	f
	.type	f, @function
f:                              ;! int(size_t)
	pushq	%rbx
	subq	$64, %rsp
	movl	(%rsp,%rdi), %eax #! stack buffer (x, %rsp + 48, %rsp + 80)
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
