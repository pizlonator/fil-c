	.text
# A by-address buffer access whose base is not a general register (%rip): the
# check validates a byte address held in a general-register web; an %rip-based
# operand has none.
	.globl	f
	.type	f, @function
f:                              ;! long(size_t)
	subq	$64, %rsp
	movq	$0, 8(%rsp) #! stack buffer (x, %rsp, %rsp + 32)
	movl	sym(%rip), %eax #! stack buffer (x)
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
