# Heap-form access through a conflicted base stays legal: on one path %rax
# holds the parked stack pointer, on the other a heap pointer — an explicit
# conflict mark. A `;! load ptr` through it passes through and is
# bounds-checked at runtime against the explicit capability, fail-closed (the
# stack+offset value on the untaken path has no valid lower, so that path
# would trap rather than access out of bounds). Plain unannotated memory
# forms through a conflicted base stay rejected (see
# sarcasm-reject-alias-ambig-att).
	.text
	.globl	confheap
	.type	confheap, @function
confheap:                       ;! long(ptr,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rsp, %rax
	testq	%rsi, %rsi
	je	.Luse
	movq	%rdi, %rax
.Luse:
	movq	(%rax), %rbx  ;! load ptr
	movq	(%rbx), %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	confheap, .-confheap
	.section	.note.GNU-stack,"",@progbits
