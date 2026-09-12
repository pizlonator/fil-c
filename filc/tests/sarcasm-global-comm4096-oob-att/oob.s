	.text
	# One past the end of the 4096-byte `.comm` traps (upper-bound check
	# on the seeded pointer's capability).
	.globl	sink_oob_store
	.type	sink_oob_store, @function
sink_oob_store:                 ;! void(long,long)
	endbr64
	leaq	aesni_mb_sink(%rip), %rax
	addq	%rdi, %rax
	movb	%sil, (%rax)
	ret
	.size	sink_oob_store, .-sink_oob_store
	.comm	aesni_mb_sink,4096,16
	.section	.note.GNU-stack,"",@progbits
