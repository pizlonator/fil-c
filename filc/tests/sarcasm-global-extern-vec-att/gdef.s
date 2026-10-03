	.text
	.globl	vec_lea_load
	.type	vec_lea_load, @function
vec_lea_load:                   ;! void(ptr)
	leaq	g(%rip), %rax #! global ptr
	movdqa	(%rax), %xmm0
	movdqu	%xmm0, (%rdi)
	leaq	g+16(%rip), %rax #! global ptr
	movdqa	(%rax), %xmm0
	movdqu	%xmm0, 16(%rdi)
	ret
	.size	vec_lea_load, .-vec_lea_load
	.globl	vec_direct_load
	.type	vec_direct_load, @function
vec_direct_load:                ;! void(ptr)
	movdqa	g(%rip), %xmm0 #! global ptr
	movdqu	%xmm0, (%rdi)
	movdqa	g+16(%rip), %xmm0 #! global ptr
	movdqu	%xmm0, 16(%rdi)
	ret
	.size	vec_direct_load, .-vec_direct_load
	.globl	vec_direct_store
	.type	vec_direct_store, @function
vec_direct_store:               ;! void(ptr)
	movdqu	(%rdi), %xmm0
	movdqa	%xmm0, g(%rip) #! global ptr
	ret
	.size	vec_direct_store, .-vec_direct_store
	.globl	vec_rmw
	.type	vec_rmw, @function
vec_rmw:                        ;! long()
	addl	$1, g+8(%rip) #! global ptr
	cmpl	$0x0f1e2d3c, g+8(%rip) #! global ptr
	sete	%al
	movzbl	%al, %eax
	ret
	.size	vec_rmw, .-vec_rmw
	.section	.note.GNU-stack,"",@progbits
