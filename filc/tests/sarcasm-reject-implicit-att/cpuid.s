/* cpuid implicitly USES eax/ecx (leaf/sub-leaf) and DEFINES FOUR registers
   (rax/rbx/rcx/rdx), none of it modeled (no register operands -> zero-effect
   unknown fallback): rejected (implicit register effects cannot be modeled). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movl	%edi, %eax
	cpuid
	movq	%rbx, %rax
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
