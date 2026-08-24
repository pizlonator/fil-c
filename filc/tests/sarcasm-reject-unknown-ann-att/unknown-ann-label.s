/* A ;! annotation on a mid-body label would be silently ignored (only the
   function-entry label carries the signature), so it is rejected. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	movq	(%rdi), %rax
.Ldone:                         ;! load ptr
	popq	%rbp
	ret
	.size	f, .-f

