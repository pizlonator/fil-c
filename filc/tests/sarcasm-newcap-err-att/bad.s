	.text
	# `new capability` is a deprecated alias for `use capability`, so it
	# follows the unified rule: address arithmetic (add/sub/lea/and/or) or
	# an instruction with a memory operand. A register-only mov is neither.
	.globl	newcap_nomem
	.type	newcap_nomem, @function
newcap_nomem:                   ;! long(ptr)
	endbr64
	movq	%rdi, %rax #! new capability %rdi
	ret
	.size	newcap_nomem, .-newcap_nomem
	.section	.note.GNU-stack,"",@progbits
