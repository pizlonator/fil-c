	.text
	# `new capability` must name the memory operand's base or index
	# register (`%rsi` is neither here).
	.globl	newcap_badreg
	.type	newcap_badreg, @function
newcap_badreg:                  ;! long(ptr)
	endbr64
	movq	(%rdi), %rax #! new capability %rsi
	ret
	.size	newcap_badreg, .-newcap_badreg
	.section	.note.GNU-stack,"",@progbits
