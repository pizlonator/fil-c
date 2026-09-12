	.text
	# An address-arithmetic instruction with a memory operand gets BOTH
	# effects from the capability selector (the ALU result draws from the
	# named source AND the access is guarded by it), so the named register
	# must be BOTH a general-register use AND the memory operand's base or
	# index. `%rax` is a use of this `add` but is neither base nor index,
	# so this is rejected: it would guard the (%rdi) access with %rax's
	# capability.
	.globl	usecap_arithmem_badreg
	.type	usecap_arithmem_badreg, @function
usecap_arithmem_badreg:         ;! long(ptr,long)
	endbr64
	addq	(%rdi), %rax #! use capability %rax
	ret
	.size	usecap_arithmem_badreg, .-usecap_arithmem_badreg
	# The deprecated `new capability` alias obeys the same dual rule.
	.globl	newcap_arithmem_badreg
	.type	newcap_arithmem_badreg, @function
newcap_arithmem_badreg:         ;! long(ptr,long)
	endbr64
	addq	(%rdi), %rax #! new capability %rax
	ret
	.size	newcap_arithmem_badreg, .-newcap_arithmem_badreg
	.section	.note.GNU-stack,"",@progbits
