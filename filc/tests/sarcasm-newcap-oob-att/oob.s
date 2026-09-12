	.text
	# Out-of-bounds through a `new capability` access still traps: the
	# annotation selects the capability, and the bounds check guards it.
	.globl	newcap_oob_store
	.type	newcap_oob_store, @function
newcap_oob_store:               ;! void(ptr,ptr,long)
	endbr64
	xorq	%rdi, %rdi
	movq	%rdx, 100(%rdi,%rsi) #! new capability %rsi
	ret
	.size	newcap_oob_store, .-newcap_oob_store
	.section	.note.GNU-stack,"",@progbits
