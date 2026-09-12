# Control-flow ambiguity on a stack+offset register: on one path %rax holds
# the parked stack pointer, on the other a heap pointer. Using it as stack+offset
# is a static error.
	.text
	.globl	ambig
	.type	ambig, @function
ambig:                          ;! long(ptr,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rsp, %rax
	testq	%rsi, %rsi
	je	.Lz
	movq	(%rdi), %rax
.Lz:
	movq	$5, 0(%rax)
	movq	%rax, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	ambig, .-ambig
	.section	.note.GNU-stack,"",@progbits
