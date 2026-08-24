/* xgetbv implicitly USES ecx (the XCR selector) and DEFINES edx:eax, none of
   it modeled (no register operands -> zero-effect unknown fallback): rejected
   like rdtsc (implicit register effects cannot be modeled). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	xorl	%ecx, %ecx
	xgetbv
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
