	.text
# A `rep movsb` carrying a `stack buffer` annotation inside a localcall clone:
# the clone's +8 return-address compensation would need its own rep alias-side
# model; rejected.
	.globl	f
	.type	f, @function
f:                              ;! void(ptr,ptr,long)
	subq	$32, %rsp
	movl	%edi, %r10d
	call	cp1
	addq	$32, %rsp
	ret
	.size	f, .-f
	.type	cp1, @function
cp1:
	leaq	(%rsp), %rsi
	movq	%rdx, %rcx
	rep movsb #! stack buffer (t, %rsp, %rsp + 16)
	ret
	.size	cp1, .-cp1
	.section	.note.GNU-stack,"",@progbits
