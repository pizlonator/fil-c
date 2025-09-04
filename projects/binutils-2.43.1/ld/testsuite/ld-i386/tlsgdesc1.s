	.text
	.globl _start
_start:
	movl	foo@tlsdesc(%ebx), %eax
	call	*foo@tlscall(%eax)
	.section	.tdata,"awT",@progbits
	.align 4
	.type	foo, @object
	.size	foo, 4
foo:
	.long	100
