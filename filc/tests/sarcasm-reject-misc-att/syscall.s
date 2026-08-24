/* syscall enters the kernel with RAW arguments, bypassing Fil-C's syscall
   argument validation — a genuine memory-safety hole — and has unmodeled
   clobbers (rcx/r11): rejected. (int3/ud2 stay allowed: pure traps.) */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movq	%rdi, %rax
	syscall
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
