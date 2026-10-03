# `0f 05` DECODES — to `syscall` — and then fails with the classifier's
# precise per-instruction rejection (system-call instructions cannot be
# bounds-checked), exactly like a spelled `syscall`. Decoding names the real
# instruction instead of reporting opaque data.
	.text
	.globl	syscallb
	.type	syscallb, @function
syscallb:                       ;! long(long)
	movq	%rdi, %rax
	.byte	0x0f,0x05
	ret
	.size	syscallb, .-syscallb
	.section	.note.GNU-stack,"",@progbits
