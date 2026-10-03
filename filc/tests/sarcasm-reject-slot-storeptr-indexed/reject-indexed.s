	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	subq	$64, %rsp
	# `;! store ptr` on an INDEXED stack access: indexed frame accesses do not
	# virtualize at all (the index could name any slot), so there is no slot
	# web to carry the capability.
	movq	%rdi, (%rsp,%rax)  ;! store ptr
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
