/* `rep movsb` parses with mnemonic "rep" (the real opcode follows as
   "operands" the model ignores), so it classified as a ZERO-effect unknown
   mnemonic: the setup moves were collapsed and the rep movsb executed with
   GARBAGE rsi/rdi/rcx — a verified catastrophic miscompile. Real compilers DO
   emit rep movsb (memcpy at -Os/ERMS), but sarcasm cannot model its implicit
   multi-register RMW: rejected with a clear message. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr, long)
	movq	%rsi, %rcx
	movq	%rdi, %rsi
	rep movsb
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
