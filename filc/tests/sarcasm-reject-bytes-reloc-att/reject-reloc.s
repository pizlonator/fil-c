# A data directive whose value is a SYMBOL/RELOCATION — `.long .Ltable` — is
# not a plain literal: it keeps its data spelling and the body is rejected
# (relocations cannot be decoded into instructions; jump tables are not
# supported).
	.text
	.globl	relocdata
	.type	relocdata, @function
relocdata:                      ;! long(long)
	movq	%rdi, %rax
	.long	.Ltable
	ret
	.size	relocdata, .-relocdata
	.section	.note.GNU-stack,"",@progbits
