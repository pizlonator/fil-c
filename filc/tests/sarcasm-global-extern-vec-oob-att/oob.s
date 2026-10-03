	.text
	.globl	vec_oob
	.type	vec_oob, @function
vec_oob:                        ;! void(ptr)
	movdqa	g+32(%rip), %xmm0 #! global ptr
	movdqu	%xmm0, (%rdi)
	ret
	.size	vec_oob, .-vec_oob
	.section	.note.GNU-stack,"",@progbits
