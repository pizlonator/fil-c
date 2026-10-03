	.text
	.globl	f
	.type	f, @function
f:                              ;! void()
	subq	$64, %rsp
	# `;! store ptr` needs a general-register source: an immediate has no
	# capability to store.
	movq	$0, -8(%rsp)       ;! store ptr
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
