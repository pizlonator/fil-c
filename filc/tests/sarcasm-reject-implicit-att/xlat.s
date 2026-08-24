/* xlat implicitly USES rbx + al (al = [rbx + zero-extended al]) and DEFINES
   al, none of it modeled (no register operands -> zero-effect unknown
   fallback): rejected (implicit register effects cannot be modeled). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movq	%rdi, %rbx
	movl	%esi, %eax
	xlat
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
