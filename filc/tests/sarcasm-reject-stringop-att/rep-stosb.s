/* `rep stosb` (memset at -Os/ERMS) parses with mnemonic "rep": zero-effect
   unknown fallback -> garbage rdi/rcx -> catastrophic miscompile, exactly
   like rep movsb. Rejected with a clear message. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr, long)
	movq	%rsi, %rcx
	movl	$0, %eax
	rep stosb
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
