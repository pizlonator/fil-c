# Returning a stack+offset register is a static error (the return value
# would be a live stack address).
	.text
	.globl	aliasret
	.type	aliasret, @function
aliasret:                       ;! long(ptr)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rsp, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	aliasret, .-aliasret
	.section	.note.GNU-stack,"",@progbits
