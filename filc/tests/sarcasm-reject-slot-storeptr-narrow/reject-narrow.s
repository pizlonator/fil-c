	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	subq	$64, %rsp
	# A 4-byte move cannot carry a capability: capabilities are 8-byte
	# (intval, lower) GPR pairs. (A 4-byte scalar store to a slot is legal
	# WITHOUT the annotation -- it just kills the slot's pointer-ness.)
	movl	%edi, -8(%rsp)     ;! store ptr
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
