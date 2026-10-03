# The 16-bit flag-stack transfers (pushfw/popfw, the 0x66 prefixes) move a
# 2-byte word (rsp shifts by 2 and only the low 16 bits of EFLAGS transfer),
# which the 8-byte-word stack model does not track. They stay rejected with
# their own message (the 64-bit pushf/pushfq/popf/popfq are modeled now).
	.text
	.globl	foo
	.type	foo, @function
foo:                            ;! long(long)
	movq	%rdi, %rax
	pushfw
	popfw
	ret
	.size	foo, .-foo
	.section	.note.GNU-stack,"",@progbits
