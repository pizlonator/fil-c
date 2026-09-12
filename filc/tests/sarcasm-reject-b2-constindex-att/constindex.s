# Constant-scale indexed stack traffic in a B2 clone (fail-closed): only
# plain base+disp slots virtualize — an indexed rsp-relative access keys no
# slot even when its scale is 1 (spelling the same address a plain access
# would name). Accepting it would alias two keyings to one address: the
# plain store below and the indexed load would desync. Static error.
	.text
	.globl	cidx_jump
	.type	cidx_jump, @function
cidx_jump:                      ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	jmp	.Lcidx_tail
	.size	cidx_jump, .-cidx_jump
	.globl	cidx_owner
	.type	cidx_owner, @function
cidx_owner:                     ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	movq	%rsi, 0(%rsp)
	nop
.Lcidx_tail:
	movq	%rsi, %rcx
	movq	$7, 0(%rsp)
	movq	0(%rsp,%rcx,1), %rax
	addq	%rbx, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	cidx_owner, .-cidx_owner
	.section	.note.GNU-stack,"",@progbits
