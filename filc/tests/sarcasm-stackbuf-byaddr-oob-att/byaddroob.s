	.file "byaddroob.c"
	.text
# By-address accesses bounds-check the RUNTIME address: flipping a high bit on
# the buffer address produces an address outside the lowered group, and the
# check rejects it with the usual `filc safety error` (the raw access would
# otherwise touch memory sarcasm does not own). The zeroing xor idiom keeps
# today's shape: an xor'd buffer address is an ordinary web.
	.globl	baoob
	.type	baoob, @function
baoob:                          ;! long(size_t)
	subq	$64, %rsp
	movq	$0, 8(%rsp) #! stack buffer (x, %rsp, %rsp + 32)
	leaq	8(%rsp), %rax
	xorq	$0x1000000, %rax
	movl	(%rax), %eax #! stack buffer (x)
	addq	$64, %rsp
	ret
	.size	baoob, .-baoob
	.section	.note.GNU-stack,"",@progbits
