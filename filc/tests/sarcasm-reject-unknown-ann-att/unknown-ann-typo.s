/* ';! lod ptr' is a typo for ';! load ptr': an unrecognized annotation is
   rejected rather than silently dropped (dropping it could skip a barrier). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	movq	(%rdi), %rax        ;! lod ptr
	popq	%rbp
	ret
	.size	f, .-f

