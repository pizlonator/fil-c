	.text
	# Out-of-bounds through a `use capability` vector access still traps:
	# the annotation selects the capability, and the bounds check guards it.
	.globl	usecapmem_oob_vstore
	.type	usecapmem_oob_vstore, @function
usecapmem_oob_vstore:           ;! void(ptr,ptr)
	endbr64
	xorq	%rdi, %rdi
	vmovdqu	%ymm0, 100(%rdi,%rsi) #! use capability %rsi
	vzeroupper
	ret
	.size	usecapmem_oob_vstore, .-usecapmem_oob_vstore
	.section	.note.GNU-stack,"",@progbits
