	.text
	# `use capability` requires an address-arithmetic instruction
	# (add/sub/lea/and/or) or an instruction with a memory operand — a
	# register-only mov is neither.
	.globl	usecap_badop
	.type	usecap_badop, @function
usecap_badop:                   ;! long(ptr)
	endbr64
	movq	%rdi, %rax #! use capability %rdi
	ret
	.size	usecap_badop, .-usecap_badop
	.section	.note.GNU-stack,"",@progbits
