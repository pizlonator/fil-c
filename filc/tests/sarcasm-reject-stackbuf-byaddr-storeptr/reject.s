	.text
# `store ptr` on an access through a buffer address: buffer bytes cannot hold
# capabilities, and the unannotated raw access through a register holding a
# buffer address is rejected the same way the static shapes are (the pointer
# annotation cannot rescue it — the value is a capability-less stack address).
	.globl	f
	.type	f, @function
f:                              ;! long(size_t)
	subq	$64, %rsp
	movq	$0, 8(%rsp) #! stack buffer (x, %rsp, %rsp + 32)
	leaq	8(%rsp), %rbx
	movq	%rdi, (%rbx) #! store ptr
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
