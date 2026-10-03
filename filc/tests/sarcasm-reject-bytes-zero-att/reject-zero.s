# `.zero`/`.space`/`.fill` emit data without spelling out bytes: they are not
# instruction encodings and are not decoded, so they keep hitting the
# data-in-a-function-body rejection.
	.text
	.globl	zerodata
	.type	zerodata, @function
zerodata:                       ;! long(long)
	movq	%rdi, %rax
	.zero	8
	ret
	.size	zerodata, .-zerodata
	.section	.note.GNU-stack,"",@progbits
