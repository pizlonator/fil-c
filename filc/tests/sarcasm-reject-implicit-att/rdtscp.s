/* rdtscp implicitly DEFINES edx:eax AND ecx (timestamp + TSC_AUX), none of it
   modeled (no register operands -> zero-effect unknown fallback): rejected
   like rdtsc (implicit register effects cannot be modeled). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	rdtscp
	shlq	$32, %rdx
	orq	%rdx, %rax
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
