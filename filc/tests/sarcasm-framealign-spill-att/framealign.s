	.text
	.globl	touch
	.type	touch, @function
touch:                          ;! void(ptr)
	movq	$1, 0(%rdi)
	ret
	.size	touch, .-touch
	.globl	align64_spill
	.type	align64_spill, @function
align64_spill:                  ;! void(ptr,ptr)
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	movq	%rsp, %rbp
	subq	$192, %rsp
	andq	$-64, %rsp
	movq	$11, %r8
	movq	$22, %r9
	movq	$33, %r10
	movq	$44, %r11
	movq	%rdi, %rax
	movq	%rsi, %rcx
	vpxorq	%zmm0, %zmm0, %zmm0
	vmovdqa64	%zmm0, 0(%rsp)
	vmovdqa64	%zmm0, 64(%rsp)
	vmovdqa64	%zmm0, 128(%rsp)
	movq	%rax, %rdi
	call	touch               ;! void(ptr)
	addq	%r8, %r9
	addq	%r10, %r11
	addq	%r9, %r11
	movq	%r11, 0(%rcx)
	leaq	(%rbp), %rsp
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbp
	popq	%rbx
	ret
	.size	align64_spill, .-align64_spill
	.section	.note.GNU-stack,"",@progbits
