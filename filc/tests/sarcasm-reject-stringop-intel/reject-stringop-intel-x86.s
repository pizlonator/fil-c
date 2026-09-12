	.intel_syntax noprefix
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(ptr, ptr)
	# Intel dword string moves/stores (movsd/stosd) lower to checked copies,
	# but the other Intel string ops (scan/compare/load) have no checked
	# lowering.
	lodsd
	mov	rax, rdi
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
