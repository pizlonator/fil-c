	.text
	# A large writable `.comm` (the aesni_mb_sink shape: 4096 bytes) becomes
	# a bounds-checked Fil-C data object. `leaq sym(%rip)` seeds a checked
	# pointer; indexed traffic through it traps out of bounds (see
	# sarcasm-global-comm4096-oob-att) and works in bounds here — including
	# at the last byte, and from C through the emitted getter.
	.globl	sink_store
	.type	sink_store, @function
sink_store:                     ;! void(long,long)
	endbr64
	leaq	aesni_mb_sink(%rip), %rax
	addq	%rdi, %rax
	movb	%sil, (%rax)
	ret
	.size	sink_store, .-sink_store
	.globl	sink_load
	.type	sink_load, @function
sink_load:                      ;! long(long)
	endbr64
	leaq	aesni_mb_sink(%rip), %rax
	addq	%rdi, %rax
	movzbq	(%rax), %rax
	ret
	.size	sink_load, .-sink_load
	.comm	aesni_mb_sink,4096,16
	.section	.note.GNU-stack,"",@progbits
