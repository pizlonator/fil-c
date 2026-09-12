	.text
	# `use capability` must name a general-register use of its own
	# instruction (`%rcx` is not an operand of this `add`).
	.globl	usecap_badreg
	.type	usecap_badreg, @function
usecap_badreg:                  ;! long(ptr,long)
	endbr64
	addq	%rsi, %rdi #! use capability %rcx
	movq	(%rdi), %rax
	ret
	.size	usecap_badreg, .-usecap_badreg
	.section	.note.GNU-stack,"",@progbits
