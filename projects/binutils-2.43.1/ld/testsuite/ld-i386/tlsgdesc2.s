	.text
	.globl _start
_start:
	leal	foo@tlsdesc(%ebx), %eax
	jmp	*foo@tlscall(%eax)
	.section	.tdata,"awT",@progbits
	.align 4
	.type	foo, @object
	.size	foo, 4
foo:
	.long	100
