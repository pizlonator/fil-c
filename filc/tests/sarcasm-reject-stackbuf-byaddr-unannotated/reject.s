	.text
# An access through a register that POSSIBLY holds a buffer byte address (one
# path materializes the buffer-interior `leaq`'s value into it, the other does
# not) without the annotation: the buffer address is a capability-less
# integer, so the ordinary checked path would trap at runtime — the
# annotation is the fix.
	.globl	f
	.type	f, @function
f:                              ;! long(size_t)
	subq	$64, %rsp
	movq	$0, 8(%rsp) #! stack buffer (x, %rsp, %rsp + 32)
	testq	%rdi, %rdi
	jz	.skip
	leaq	8(%rsp), %rbx
.skip:
	movl	(%rbx), %eax
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
