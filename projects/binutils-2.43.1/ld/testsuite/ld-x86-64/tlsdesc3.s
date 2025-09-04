	.text
	.globl	_start
	.type	_start,@function
_start:
	movq	foo@tlsdesc(%rip), %rax
	call	*foo@tlscall(%rax)
	.globl foo
	.section	.tdata,"awT",@progbits
	.align 8
	.type	foo, @object
	.size	foo, 8
foo:
	.quad	100
