/* rdtsc implicitly DEFINES edx:eax (the timestamp), but it has no register
   operands, so the unknown-mnemonic fallback modeled ZERO register effects and
   disconnected the rdx/rax webs — a verified miscompile: the program's `shlq
   $32, %rdx` could be reallocated to a register rdtsc never wrote. Rejected. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	rdtsc
	shlq	$32, %rdx
	orq	%rdx, %rax
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
