/* lahf implicitly DEFINES %rax (SF/ZF/AF/PF/CF -> AH), but it has no register
   operands, so the unknown-mnemonic fallback modeled ZERO register effects and
   silently disconnected the %rax web (a miscompile under register pressure,
   verified end-to-end): rejected with a clear message. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movq	%rdi, %rax
	lahf
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
