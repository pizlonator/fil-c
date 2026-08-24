/* daa (like aaa/aas/das/aam/aad) implicitly read-modify-writes ax (and the
   flags), none of it modeled (no register operands -> zero-effect unknown
   fallback): rejected (implicit register effects cannot be modeled). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movl	%edi, %eax
	daa
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
