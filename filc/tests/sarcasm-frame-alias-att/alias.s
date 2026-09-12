# Stack+offset alias registers: `movq %rsp, %reg` parks the stack pointer and
# `leaq K(%rsp), %reg` parks stack+offset; accesses at offsets from the
# register are ordinary stack accesses (virtualized into slot webs here).
	.text
	.globl	alias_mov
	.type	alias_mov, @function
alias_mov:                      ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rsp, %rax
	movq	%rdi, 0(%rax)
	movq	%rsi, 8(%rax)
	movq	$123, 16(%rax)
	movq	0(%rax), %rcx
	addq	8(%rax), %rcx
	addq	16(%rax), %rcx
	movq	%rcx, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	alias_mov, .-alias_mov
	.globl	alias_lea
	.type	alias_lea, @function
alias_lea:                      ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	leaq	-32(%rsp), %rax
	movq	%rdi, 32(%rax)
	movq	%rsi, 40(%rax)
	movq	32(%rax), %rcx
	addq	40(%rax), %rcx
	movq	%rcx, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	alias_lea, .-alias_lea
	.section	.note.GNU-stack,"",@progbits
