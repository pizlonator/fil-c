/* `repe cmpsb` parses with mnemonic "repe": its implicit rsi/rdi/rcx RMW and
   ZF write are unmodeled: rejected like rep (string instructions have
   implicit register effects; use explicit load/store loops). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(ptr, long)
	movq	%rsi, %rcx
	movq	%rdi, %rsi
	repe cmpsb
	movl	$0, %eax
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
